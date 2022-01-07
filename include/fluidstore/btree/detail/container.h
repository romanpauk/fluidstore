#pragma once

#include <memory>
#include <type_traits>
#include <algorithm>
#include <iterator>

#define BTREE_VALUE_NODE_LR

// TODO: append optimization has a lot of issues...
#define BTREE_VALUE_NODE_APPEND

#if defined(_DEBUG)
    //#define BTREE_CHECK_VECTOR_INVARIANTS
    #define BTREE_CHECK_TREE_INVARIANTS

    // TODO: try lambdas, but look at code generated for release.
    #define BTREE_CHECK(code) \
        do { \
            (code); \
            this->checktree(); \
        } while(0)

    #define BTREE_CHECK_RETURN(code) \
        do { \
            auto result = (code); \
            this->checktree(); \
            return result; \
        } while(0)
#else
#define BTREE_CHECK(code) (code)
#define BTREE_CHECK_RETURN(code) return (code);
#endif

#include <fluidstore/btree/detail/fixed_vector.h>

namespace btree::detail
{    
    // This class will not have any data, it is there just to have common base class for both node types.
    struct node
    {
    protected:
        node() = default;

    #if defined(_DEBUG)
        virtual ~node() {}
    #endif
    };

    template < typename Node > struct node_descriptor;

    template < typename Node, typename SizeType, SizeType Capacity > struct keys_descriptor
    {
        using size_type = SizeType;

        keys_descriptor(Node node)
            : node_(node)
        {}

        size_type size() const { return node_->size; }
        constexpr size_type capacity() const { return Capacity; }

        void set_size(size_type size)
        {
            assert(size <= capacity());
            node_->size = size;
        }

        void* data() { return reinterpret_cast<void*>(node_->keys); }
        const void* data() const { return reinterpret_cast<const void*>(node_->keys); }

    private:
        Node node_;
    };

    template < typename Node, typename SizeType, SizeType Capacity > struct children_descriptor
    {
        using size_type = SizeType;

        children_descriptor(Node node)
            : node_(node)
            , size_()
        {
            auto keys = node_descriptor< Node >(node_).get_keys();
            if (!keys.empty())
            {
                size_ = keys.size() + 1;
            }
        }

        children_descriptor(const children_descriptor< Node, SizeType, Capacity >& other) = default;

        size_type size() const { return size_; }

        void set_size(size_type size)
        {
            assert(size <= capacity());
            size_ = size;
        }

        constexpr size_type capacity() const { return Capacity; }

        void* data() { return reinterpret_cast<void*>(node_->children); }
        const void* data() const { return reinterpret_cast<void*>(node_->children); }

    private:
        Node node_;
        size_type size_;
    };

    template < typename Node, typename SizeType, SizeType Capacity > struct values_descriptor
    {
        using size_type = SizeType;

        values_descriptor(Node node)
            : node_(node)
            , size_(node_descriptor< Node >(node_).get_keys().size())
        {}

        values_descriptor(const values_descriptor< Node, SizeType, Capacity >& other) = default;

        size_type size() const { return size_; }

        void set_size(size_type size)
        {
            assert(size <= capacity());
            size_ = size;
        }

        constexpr size_type capacity() const { return Capacity; }

        void* data() { return reinterpret_cast<void*>(node_->values); }
        const void* data() const { return reinterpret_cast<const void*>(node_->values); }

    private:
        Node node_;
        size_type size_;
    };

    template< typename SizeType, typename Key > struct node_dimension
    {
        static constexpr const auto value = std::max(std::size_t(8), std::size_t(64 / sizeof(Key))) / 2;
        static_assert((1ull << sizeof(SizeType) * 8) > 2 * value);
    };

    template < typename Key, typename Value > struct value_type_traits
    {
        typedef std::pair< Key, Value > value_type;
        typedef std::pair< const Key&, Value& > reference;

        template < typename Pair > static const Key& get_key(Pair&& p) { return p.first; }

        template < typename T > static reference* reference_address(T&& p)
        {
            // TODO: this traits pair< Key&, Value& > as POD. Needed to return address of temporary pair< Key&, Value& > for iterator::operator ->().
            // This can back fire as multiple calls to -> will overwrite the reference.
            
            static thread_local std::aligned_storage_t< sizeof(reference) > storage;
            new (&storage) reference(std::forward< T >(p));
            return reinterpret_cast<reference*>(&storage);
        }
    };

    template < typename Key > struct value_type_traits< Key, void >
    {
        typedef Key value_type;
        typedef const Key& reference;

        static const Key& get_key(const Key& p) { return p; }

        template < typename T > static std::remove_reference_t< T >* reference_address(T&& p) { return &p; }
    };

    template <
        typename Key,
        typename Value,
        typename Compare,
        typename Allocator,
        typename NodeSizeType,
        typename InternalNode,
        typename ValueNode
    > class container_base
    {
    public:
        using node_size_type = NodeSizeType;
        using size_type = size_t;
        using key_type = Key;
        using value_type = typename value_type_traits< Key, Value >::value_type;
        using reference = typename value_type_traits< Key, Value >::reference;
        using allocator_type = Allocator;
        using container_type = container_base< Key, Value, Compare, Allocator, NodeSizeType, InternalNode, ValueNode >;
        using internal_node = InternalNode;
        using value_node = ValueNode;
        using value_type_traits_type = value_type_traits< Key, Value >;

        template < bool Inc > struct depth_check
        {
            depth_check(size_type& gdepth, size_type& ldepth)
                : global_depth_(gdepth)
                , local_depth_(ldepth)
                , begin_depth_(gdepth)
            {}

            ~depth_check()
            {
                if (begin_depth_ != global_depth_)
                {
                    local_depth_ += Inc ? 1 : -1;
                }
            }

            size_type& global_depth_;
            size_type& local_depth_;
            size_type begin_depth_;
        };

        template < typename Node > static auto desc(Node n) { return node_descriptor< Node >(n); }

    public:
        struct iterator
        {
            friend container_type;
            
            using iterator_category = std::bidirectional_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = value_type;
            using pointer = value_type*;
            using reference = reference;
            
            iterator() = default;
            iterator(const iterator&) = default;

            iterator(value_node* n, node_size_type kindex)
                : node_(n)
                , kindex_(kindex)
            {}

            bool operator == (const iterator& rhs) const { return node_ == rhs.node_ && kindex_ == rhs.kindex_; }
            bool operator != (const iterator& rhs) const { return !(*this == rhs); }

            const reference operator*() const { return node_descriptor< value_node* >(node_).get_data()[kindex_]; }
            reference operator*() { return node_descriptor< value_node* >(node_).get_data()[kindex_]; }

            const std::remove_reference_t< reference >* operator->() const { return value_type_traits_type::reference_address(node_descriptor< value_node* >(node_).get_data()[kindex_]); }
            std::remove_reference_t< reference >* operator->() { return value_type_traits_type::reference_address(node_descriptor< value_node* >(node_).get_data()[kindex_]); }

            iterator& operator++()
            {
                auto node = node_descriptor< value_node* >(node_);
                assert(node_);
                assert(kindex_ <= node.get_keys().size());

                if (++kindex_ == node.get_keys().size())
                {
                #if defined(BTREE_VALUE_NODE_LR)
                    if (node_->right)
                    {
                        node_ = node_->right;
                        kindex_ = 0;
                    }
                #else
                    auto [right, rindex] = get_right(node, get_index(node), true);
                    if (right)
                    {
                        node_ = right;
                        kindex_ = 0;
                    }
                #endif
                }

                return *this;
            }

            iterator operator++(int)
            {
                iterator it = *this;
                ++* this;
                return it;
            }

            iterator& operator--()
            {
                assert(node_);
                                
                if (kindex_ == 0)
                {
                #if defined(BTREE_VALUE_NODE_LR)
                    assert(node_->left);
                    node_ = node_->left;
                #else
                    auto node = node_descriptor< value_node* >(node_);
                    std::tie(node_, tmp) = get_left(node, get_index(node), true);
                #endif
                    kindex_ = node_descriptor< value_node* >(node_).get_keys().size() - 1;
                }
                else
                {
                    --kindex_;
                }

                return *this;
            }

            iterator operator--(int)
            {
                iterator it = *this;
                --* this;
                return it;
            }

        private:
            value_node* node_;
            node_size_type kindex_;
        };

        using const_iterator = iterator;

        container_base()
            : root_()
            , first_node_()
            , last_node_()
            , size_()
            , depth_()
        {
            // TODO: why does defaulting this not clear the pointers?
        #if defined(_DEBUG)
            // Make sure the objects alias.
            value_node v;
            assert(&v == (node*)&v);
            internal_node n;
            assert(&n == (node*)&n);
        #endif
        }

        container_base(container_type&& other)
            : root_()
            , first_node_()
            , last_node_()
            , size_()
            , depth_()
        {
            operator = (std::move(other));
        }

        container_type& operator = (container_type&& other)
        {
            // TODO: it is not clear who clears the memory in case of move, as here we do not have an allocator.
            // Interestingly, no leak is reported, although this assert fails.
            //
            // assert(empty());

            std::swap(root_, other.root_);
            std::swap(first_node_, other.first_node_);
            std::swap(last_node_, other.last_node_);
            std::swap(size_, other.size_);
            std::swap(depth_, other.depth_);
            return *this;
        }

        container_base(const container_type&& other) = delete;
        container_type& operator = (const container_type& other) = delete;

    #if defined(_DEBUG)
        ~container_base()
        {
            assert(empty());
        }
    #else
        ~container_base() = default;
    #endif  

        template < typename AllocatorT > void clear(AllocatorT& allocator)
        {
            if (root_)
            {
                free_node(allocator, root_, 1);

                root_ = first_node_ = last_node_ = nullptr;
                depth_ = size_ = 0;
            }
        }

        iterator find(const key_type& key) const
        {
            return root_ ? find(root_, key) : end();
        }

        iterator lower_bound(const key_type& key) const
        {
            return root_ ? lower_bound(root_, key) : end();
        }

        template < typename AllocatorT > std::pair< iterator, bool > insert(AllocatorT& allocator, const value_type& value)
        {
            return emplace(allocator, value);
        }      

        template < typename AllocatorT > std::pair< iterator, bool > insert(AllocatorT& allocator, const_iterator hint, const value_type& value)
        {
            return emplace_hint(allocator, hint, value);
        }

        template < typename AllocatorT, typename It > void insert(AllocatorT& allocator, It begin, It end)
        {
            while (begin != end)
            {
                insert(allocator, *begin++);
            }
        }

        template < typename AllocatorT, typename It > void insert(AllocatorT& allocator, const_iterator pos, It begin, It end)
        {
            while (begin != end)
            {
                pos = insert(allocator, pos + 1, *begin++).first;
            }
        }

        template < typename AllocatorT > void erase(AllocatorT& allocator, const key_type& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase(allocator, it);
            }
        }

        template < typename AllocatorT > iterator erase(AllocatorT& allocator, iterator it)
        {
            // TODO: size should be decremented after successfull erase

            assert(it != end());

            auto node = desc(it.node_);
            auto ndata = node.get_data();

            if (node.get_parent())
            {
                if (ndata.size() > ndata.capacity() / 2)
                {
                    --size_;
                    if (ndata.erase(allocator, ndata.begin() + it.kindex_) == ndata.end())
                    {
                        auto [right, rindex] = get_right(node, get_index(node), true);
                        if (right)
                        {
                            return iterator(right, 0);
                        }
                    }

                    return it;
                }
                else
                {
                    auto key = node.get_keys()[it.kindex_];
                    auto [n, nindex] = rebalance_erase(allocator, depth_, node, get_index(node));

                    assert(find_key_index(n.get_keys(), key) < n.get_keys().size());

                    auto ndata = n.get_data();
                    auto kindex = find_key_index(n.get_keys(), key);

                    --size_;
                    if (ndata.erase(allocator, ndata.begin() + kindex) == ndata.end())
                    {
                        auto [right, rindex] = get_right(n, nindex, true);
                        if (right)
                        {
                            return iterator(right, 0);
                        }
                    }

                    return iterator(n, kindex);
                }
            }
            else
            {
                --size_;
                if (ndata.erase(allocator, ndata.begin() + it.kindex_) == ndata.end())
                {
                    if (empty())
                    {
                        // TODO: we should keep what we allocated
                        clear(allocator);
                    }

                    return end();
                }

                return it;
            }
        }

        size_type size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() const { return iterator(first_node_, 0); }
        iterator end() const { return iterator(last_node_, last_node_ ? last_node_->size : 0); }
        
        template < typename AllocatorT, typename... Args > std::pair< iterator, bool > emplace(AllocatorT& allocator, Args&&... args)
        {
            value_type value{ std::forward< Args >(args)... };

            if (!root_)
            {
                auto root = allocate_node< value_node >(allocator);
                root_ = first_node_ = last_node_ = root;
                depth_ = 1;

                return insert(allocator, root, 0, std::move(value));
            }

            const auto& key = value_type_traits_type::get_key(value);
            auto [n, nindex] = find_value_node(root_, key);
            if (!full(n))
            {
                return insert(allocator, n, std::move(value));
            }
            else
            {
                if (n.get_parent())
                {
                    std::tie(n, nindex) = rebalance_insert(allocator, depth_, n, nindex, key);
                    return insert(allocator, n, std::move(value));
                }
                else
                {
                    std::tie(n, nindex) = rebalance_insert(allocator, depth_, n, key);
                    return insert(allocator, n, std::move(value));
                }
            }
        }

        template < typename AllocatorT, typename... Args > std::pair< iterator, bool > emplace_hint(AllocatorT& allocator, const_iterator hint, Args&&... args)
        {
            if (empty())
            {
                assert(hint == end());
                return emplace(allocator, std::forward< Args >(args)...);
            }
         
            // hint is an iterator to an element that we are going to insert BEFORE.
            assert(!empty());
                                    
            value_type value{ std::forward< Args >(args)... };
            const auto& key = value_type_traits_type::get_key(value);

            Compare compare;
            iterator prev = hint;

            // check hint validity
            if(hint == end())
            {
                --prev;

                if(!compare(value_type_traits_type::get_key(*prev), key))
                {
                    assert(false);
                    return emplace(allocator, std::move(value));
                }
            } 
            else if (hint == begin())
            {
                if (!compare(key, value_type_traits_type::get_key(*hint)))
                {
                    assert(false);
                    return emplace(allocator, std::move(value));
                }
            }
            else
            {
                --prev;

                assert(!empty());
                assert(hint != begin());

                if(!(compare(value_type_traits_type::get_key(*prev), key) && compare(key, value_type_traits_type::get_key(*hint))))
                {
                    assert(false);
                    return emplace(allocator, std::move(value));
                }
            }
            
            auto n = desc(hint.node_);
            node_size_type nindex = 0;

            if(!full(n))
            {
                return insert(allocator, n, hint.kindex_, std::move(value));
            }
            else
            {
                std::tie(n, nindex) = rebalance_insert(allocator, depth_, n, key);
                return insert(allocator, n, std::move(value));
            }    
        }

    protected:
        std::tuple< node_descriptor< value_node* >, node_size_type > find_value_node(node* n, const key_type& key) const
        {
            size_type depth = depth_;
            node_size_type nindex = 0;
            assert(depth_ > 0);
            while (--depth)
            {
                auto in = desc(node_cast<internal_node*>(n));

                auto nkeys = in.get_keys();
                auto kindex = find_key_index(nkeys, key);
                if (kindex != nkeys.size())
                {
                    nindex = static_cast<node_size_type>(kindex + !compare_lte(key, nkeys[kindex]));
                }
                else
                {
                    nindex = nkeys.size();
                }

                n = in.template get_children< node* >()[nindex];
            }

            return { node_cast<value_node*>(n), nindex };
        }

        iterator find(node* n, const key_type& key) const
        {
            // TODO: implement find using lower_bound
            auto [vn, vnindex] = find_value_node(n, key);
            assert(vn);

            auto nkeys = vn.get_keys();
            auto kindex = find_key_index(nkeys, key);
            if (kindex < nkeys.size() && key == nkeys[kindex])
            {
                return iterator(vn, kindex);
            }
            else
            {
                return end();
            }
        }

        iterator lower_bound(node* n, const key_type& key) const
        {
            auto [vn, vnindex] = find_value_node(n, key);
            assert(vn);

            auto nkeys = vn.get_keys();
            auto kindex = find_key_index(nkeys, key);
            if (kindex < nkeys.size())
            {
                return iterator(vn, kindex);
            }
            else
            {
                return end();
            }
        }

        // TODO: upper_bound

        template < typename AllocatorT, typename T > std::pair< iterator, bool > insert(AllocatorT& allocator, node_descriptor< value_node* > n, T&& value)
        {
            assert(!full(n));

            const auto& key = value_type_traits_type::get_key(value);

            auto nkeys = n.get_keys();
            auto kindex = find_key_index(nkeys, key);
            if (kindex < nkeys.size() && key == nkeys[kindex])
            {
                return { iterator(n, kindex), false };
            }
            else
            {
                return insert(allocator, n, kindex, std::forward< T >(value));
            }
        }

        template < typename AllocatorT, typename T > std::pair< iterator, bool > insert(AllocatorT& allocator, node_descriptor< value_node* > n, node_size_type kindex, T&& value)
        {
            assert(!full(n));

            n.get_data().emplace(allocator, n.get_data().begin() + kindex, std::forward< T >(value));
            ++size_;
            return { iterator(n, kindex), true };
        }

        template < typename Descriptor > auto find_key_index(const fixed_vector< Key, Descriptor >& keys, const key_type& key) const
        {
            Compare compare;

            // TODO: better search
            auto index = keys.begin();
            for (; index != keys.end(); ++index)
            {
                if (!compare(*index, key))
                {
                    break;
                }
            }

            return static_cast<node_size_type>(index - keys.begin());
        }

        template < typename Node, typename Descriptor > static auto find_node_index(const fixed_vector< Node*, Descriptor >& nodes, const node* n)
        {
            // TODO: better search, this should use vector with tombstone so we can iterate over capacity() and unroll the iteration.
            for (decltype(nodes.size()) i = 0; i < nodes.size(); ++i)
            {
                if (nodes[i] == n)
                {
                    return i;
                }
            }

            return nodes.size();
        }

        template< typename AllocatorT, typename Node > void remove_node(AllocatorT& allocator, size_type depth, node_descriptor< internal_node* > parent, const node_descriptor< Node* > n, node_size_type nindex, node_size_type kindex)
        {
            auto pchildren = parent.template get_children< node* >();
            pchildren.erase(allocator, pchildren.begin() + nindex);

            auto pkeys = parent.get_keys();
            pkeys.erase(allocator, pkeys.begin() + kindex);

            if (pkeys.empty())
            {
                auto root = root_;
                fixed_vector< node*, children_descriptor< internal_node*, size_type, 2 * internal_node::N > > pchildren((internal_node*)parent, 1); // override the size to 1
                root_ = pchildren[0];
                --depth_;
                assert(depth_ >= 1);
                set_parent(depth_ == 1, root_, nullptr);

                deallocate_node(allocator, desc(node_cast<internal_node*>(root)));
            }
        }

        template < typename Node, typename AllocatorT > auto get_node_allocator(AllocatorT& allocator)
        {
            return typename std::allocator_traits< AllocatorT >::template rebind_alloc< Node >(allocator);
        }

        template < typename Node, typename AllocatorT > Node* allocate_node(AllocatorT& allocator)
        {
            static_assert(!std::is_same_v<Node, node>);

            auto alloc = get_node_allocator< Node >(allocator);
            auto ptr = std::allocator_traits< decltype(alloc) >::allocate(alloc, 1);
            std::allocator_traits< decltype(alloc) >::construct(alloc, ptr);
            return ptr;
        }

        template < typename AllocatorT, typename Node > void deallocate_node(AllocatorT& allocator, node_descriptor< Node* > n)
        {
            static_assert(!std::is_same_v<Node, node>);

            n.cleanup(allocator);

            auto alloc = get_node_allocator< Node >(allocator);
            std::allocator_traits< decltype(alloc) >::destroy(alloc, n.node());
            std::allocator_traits< decltype(alloc) >::deallocate(alloc, n.node(), 1);
        }

        static const Key& split_key(/*const*/ node_descriptor< internal_node* > n)
        {
            assert(full(n));
            return *(n.get_keys().begin() + internal_node::N);
        }

        template < typename AllocatorT > std::tuple< node_descriptor< internal_node* >, key_type > split_node(AllocatorT& allocator, size_type depth, node_descriptor< internal_node* > lnode, node_size_type lindex, const key_type&)
        {
            assert(full(lnode));
            auto rnode = desc(allocate_node< internal_node >(allocator));

            auto lchildren = lnode.template get_children< node* >();
            auto rchildren = rnode.template get_children< node* >();

            assert(depth_ >= depth + 1);
            rchildren.insert(allocator, rchildren.begin(), lchildren.begin() + internal_node::N, lchildren.end());
            std::for_each(lchildren.begin() + internal_node::N, lchildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, rnode); });

            auto lkeys = lnode.get_keys();
            auto rkeys = rnode.get_keys();

            auto begin = lkeys.begin() + internal_node::N;
            rkeys.insert(allocator, rkeys.end(), std::make_move_iterator(begin), std::make_move_iterator(lkeys.end()));

            Key splitkey = std::move(*(begin - 1));

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(allocator, begin - 1, lkeys.end());
            assert(lkeys.size() == internal_node::N - 1);
            assert(rkeys.size() == internal_node::N - 1);

            return { rnode, splitkey };
        }

        template < typename AllocatorT > std::tuple< node_descriptor< value_node* >, key_type > split_node(AllocatorT& allocator, size_type, node_descriptor< value_node* > lnode, node_size_type lindex, const key_type& key)
        {
            assert(full(lnode));
            auto rnode = desc(allocate_node< value_node >(allocator));
            auto lkeys = lnode.get_keys();
            auto ldata = lnode.get_data();

        #if defined(BTREE_VALUE_NODE_APPEND)
            // In case of appending to the right-most node, we will allow rnode to be empty as there might be more appends comming.
            // The tree will be very slightly disbalanced on its right edge but that is ok, it does not impact the overall complexity.
            // The node needs to be properly split and items distributed if there is a right node or the operation is not an append.
            // TODO: this needs equivalent fixes for erase as the code is not used to unbalanced nodes.
            auto [right, rindex] = get_right(lnode, lindex);
            if (right || compare_lte(key, lkeys.back()))
            {
        #endif
                auto rdata = rnode.get_data();

                auto begin = ldata.begin() + value_node::N;
                rdata.insert(allocator, rdata.end(), std::make_move_iterator(begin), std::make_move_iterator(ldata.end()));

                // Keep splitkey.
                ldata.erase(allocator, begin, ldata.end());
                assert(ldata.size() == value_node::N);
                assert(rdata.size() == value_node::N);

        #if defined(BTREE_VALUE_NODE_APPEND)
            }
        #endif

        #if defined(BTREE_VALUE_NODE_LR)
            auto znode = lnode.node()->right;
            lnode.node()->right = rnode;
            rnode.node()->right = znode;
            rnode.node()->left = lnode;
            if (znode)
            {
                znode->left = rnode;
            }
        #endif

            return { rnode, lkeys.back() };
        }

        template < typename AllocatorT > void free_node(AllocatorT& allocator, node* n, size_type depth)
        {
            if (depth != depth_)
            {
                auto in = desc(node_cast<internal_node*>(n));
                auto nchildren = in.template get_children< node* >();

                for (auto child : nchildren)
                {
                    free_node(allocator, child, depth + 1);
                }

                deallocate_node(allocator, in);
            }
            else
            {
                deallocate_node(allocator, desc(node_cast<value_node*>(n)));
            }
        }

        template < typename Node > static Node first_node(node* n, size_type depth)
        {
            assert(n);
            while (--depth)
            {
                n = desc(node_cast<internal_node*>(n)).template get_children< node* >()[0];
            }

            return node_cast<Node>(n);
        }

        template < typename Node > static Node last_node(node* n, size_type depth)
        {
            assert(n);
            assert(depth > 0);
            while (--depth)
            {
                auto children = desc(node_cast<internal_node*>(n)).template get_children< node* >();
                n = children[children.size() - 1];
            }

            return node_cast<Node>(n);
        }

        template < typename AllocatorT > bool share_keys(AllocatorT& allocator, size_t, node_descriptor< value_node* > target, node_size_type tindex, node_descriptor< value_node* > source, node_size_type sindex)
        {
            if (!source)
            {
                return false;
            }

            auto sdata = source.get_data();
            auto tdata = target.get_data();
            auto pkeys = target.get_parent().get_keys();

            if (sdata.size() > value_node::N && tdata.size() < 2 * value_node::N)
            {
                if (tindex > sindex)
                {
                    // Right-most key from the left node
                    auto key = sdata.end() - 1;
                    tdata.emplace(allocator, tdata.begin(), std::move(*key));
                    sdata.erase(allocator, key);

                    assert(tindex > 0);
                    pkeys[tindex - 1] = source.get_keys().back();
                }
                else
                {
                    // Left-most key from the right node
                    auto key = sdata.begin();
                    tdata.emplace(allocator, tdata.end(), std::move(*key));
                    sdata.erase(allocator, key);

                    pkeys[tindex] = target.get_keys().back();
                }

                return true;
            }

            if (source.node() == last_node_ && !sdata.empty())
            {
                // Left-most key from the right node
                auto key = sdata.begin();
                tdata.emplace(allocator, tdata.end(), std::move(*key));
                sdata.erase(allocator, key);

                pkeys[tindex] = target.get_keys().back();

                // TODO: not a good place for this
                if (sdata.empty())
                {
                    last_node_ = target;
                    target.node()->right = nullptr;
                }

                return true;
            }

            return false;
        }

        template < typename AllocatorT > bool share_keys(AllocatorT& allocator, size_type depth, node_descriptor< internal_node* > target, node_size_type tindex, node_descriptor< internal_node* > source, node_size_type sindex)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source.get_keys();
            auto tkeys = target.get_keys();

            assert(depth_ >= depth + 1);

            if (skeys.size() > internal_node::N - 1 && tkeys.size() < 2 * internal_node::N - 1)
            {
                auto pkeys = target.get_parent().get_keys();

                auto schildren = source.template get_children< node* >();
                auto tchildren = target.template get_children< node* >();

                if (tindex > sindex)
                {
                    tchildren.emplace(allocator, tchildren.begin(), schildren[skeys.size()]);
                    set_parent(depth_ == depth + 1, schildren[skeys.size()], target);

                    tkeys.emplace(allocator, tkeys.begin(), std::move(pkeys[sindex]));

                    pkeys[sindex] = std::move(skeys.back());
                    skeys.erase(allocator, skeys.end() - 1);
                }
                else
                {
                    tkeys.emplace(allocator, tkeys.end(), std::move(pkeys[tindex]));

                    auto ch = schildren[0];
                    set_parent(depth_ == depth + 1, ch, target);

                    schildren.erase(allocator, schildren.begin());
                    tchildren.emplace(allocator, tchildren.end(), ch);

                    pkeys[tindex] = std::move(*skeys.begin());
                    skeys.erase(allocator, skeys.begin());
                }

                return true;
            }

            return false;
        }

        template < typename AllocatorT > bool merge_keys(AllocatorT& allocator, size_type depth, node_descriptor< value_node* > target, node_size_type tindex, node_descriptor< value_node* > source, node_size_type sindex)
        {
            if (!target)
            {
                return false;
            }

            auto tdata = target.get_data();
            if (tdata.size() == value_node::N)
            {
                auto sdata = source.get_data();
                tdata.insert(allocator, tindex < sindex ? tdata.end() : tdata.begin(), std::make_move_iterator(sdata.begin()), std::make_move_iterator(sdata.end()));

                if (tindex < sindex)
                {
                #if defined(BTREE_VALUE_NODE_LR)
                    target.node()->right = source.node()->right;
                    if (source.node()->right)
                    {
                        source.node()->right->left = target;
                    }
                #endif

                    if (last_node_ == source)
                    {
                        last_node_ = target;
                    }
                }
                else
                {
                #if defined(BTREE_VALUE_NODE_LR)
                    target.node()->left = source.node()->left;
                    if (source.node()->left)
                    {
                        source.node()->left->right = target;
                    }
                #endif

                    if (first_node_ == source)
                    {
                        first_node_ = target;
                    }
                }

                return true;
            }

            return false;
        }

        template < typename AllocatorT > bool merge_keys(AllocatorT& allocator, size_type depth, node_descriptor< internal_node* > target, node_size_type tindex, node_descriptor< internal_node* > source, node_size_type sindex)
        {
            if (!target)
            {
                return false;
            }

            auto skeys = source.get_keys();
            auto tkeys = target.get_keys();

            if (tkeys.size() == internal_node::N - 1)
            {
                auto pkeys = target.get_parent().get_keys();

                assert(depth_ >= depth + 1);

                auto schildren = source.template get_children< node* >();
                auto tchildren = target.template get_children< node* >();

                if (tindex < sindex)
                {
                    tchildren.insert(allocator, tchildren.end(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator, tkeys.end(), pkeys[sindex - 1]);
                    tkeys.insert(allocator, tkeys.end(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                }
                else
                {
                    tchildren.insert(allocator, tchildren.begin(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator, tkeys.begin(), pkeys[sindex]);
                    tkeys.insert(allocator, tkeys.begin(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                }

                return true;
            }

            return false;
        }

        template < typename AllocatorT, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_insert(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, const Key& key)
        {
            assert(full(n));
            if (n.get_parent())
            {
                return rebalance_insert(allocator, depth, n, get_index(n), key);
            }
            else
            {
                auto root = desc(allocate_node< internal_node >(allocator));
                auto [p, splitkey] = split_node(allocator, depth, n, 0, key);

                auto children = root.template get_children< node* >();
                children.emplace_back(allocator, n);
                n.set_parent(root);
                children.emplace_back(allocator, p);
                p.set_parent(root);

                if (root_ == last_node_)
                {
                    last_node_ = node_cast<value_node*>(p);
                }

                auto cmp = !(compare_lte(key, splitkey));
                root.get_keys().emplace_back(allocator, std::move(splitkey));

                root_ = root;
                ++depth_;

                return { cmp ? p : n, cmp };
            }
        }

        template < typename AllocatorT, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_insert(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex, const Key& key)
        {
            assert(full(n));
            assert(n.get_parent());

            auto parent_rebalance = full(n.get_parent());
            if (parent_rebalance)
            {
                assert(depth > 1);
                depth_check< true > dc(depth_, depth);
                rebalance_insert(allocator, depth - 1, n.get_parent(), split_key(n.get_parent()));
                assert(!full(n.get_parent()));
            }

            // TODO: check that the key is still valid reference, it might not be after split
            auto [p, splitkey] = split_node(allocator, depth, n, nindex, key);

            auto pchildren = n.get_parent().template get_children< node* >();
            if (parent_rebalance)
            {
                nindex = get_index(n);
            }

            pchildren.emplace(allocator, pchildren.begin() + nindex + 1, p);
            p.set_parent(n.get_parent());

            auto pkeys = n.get_parent().get_keys();
            pkeys.emplace(allocator, pkeys.begin() + nindex, splitkey);

            // TODO:
            if ((uintptr_t)last_node_ == (uintptr_t)n.node())
            {
                last_node_ = node_cast<value_node*>(p.node());
            }

            auto cmp = !(compare_lte(key, splitkey));
            return { cmp ? p : n, nindex + cmp };
        }

        template < typename AllocatorT, typename Node > void rebalance_erase(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n)
        {
            if (n.get_parent())
            {
                rebalance_erase(allocator, depth, n, get_index(n));
            }
        }

        template < typename AllocatorT, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_erase(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex)
        {
            assert(n.get_parent());
            assert(n.get_keys().size() <= n.get_keys().capacity() / 2);

            {
                auto [left, lindex] = get_left(n, nindex);
                if (left && share_keys(allocator, depth, n, nindex, left, lindex))
                {
                #if defined(BTREE_VALUE_NODE_APPEND)
                    // TODO: investigate - right was 0, so possibly rigthtmost node append optimization?
                    //assert(n.get_keys().size() > n.get_keys().capacity() / 2);
                #else
                    assert(n.get_keys().size() > n.get_keys().capacity() / 2);
                #endif
                    return { n, nindex };
                }
            }

            {
                auto [right, rindex] = get_right(n, nindex);
                if (right && share_keys(allocator, depth, n, nindex, right, rindex))
                {
                #if defined(BTREE_VALUE_NODE_APPEND)
                    // TODO: investigate - right was 0, so possibly rigthtmost node append optimization?
                    if (right.get_keys().empty())
                    {                        
                        remove_node(allocator, depth, n.get_parent(), right, rindex, nindex);
                        deallocate_node(allocator, right);
                    }
                #else
                    assert(n.get_keys().size() > n.get_keys().capacity() / 2);
                #endif
                    return { n, nindex };
                }
            }

            {
                auto pkeys = n.get_parent().get_keys();
                if (pkeys.size() <= pkeys.capacity() / 2)
                {
                    depth_check< false > dc(depth_, depth);
                    rebalance_erase(allocator, depth - 1, n.get_parent());
                    nindex = get_index(n);
                }
            }

            {
                // TODO: call get_left only if nindex changed
                auto [left, lindex] = get_left(n, nindex);
                if (merge_keys(allocator, depth, left, lindex, n, nindex))
                {
                    remove_node(allocator, depth, n.get_parent(), n, nindex, nindex - 1);
                    deallocate_node(allocator, n);
                    return { left, nindex - 1 };
                }
            }

            {
                // TODO: call get_right only if nindex changed
                auto [right, rindex] = get_right(n, nindex);
                if (merge_keys(allocator, depth, right, rindex, n, nindex))
                {
                    remove_node(allocator, depth, n.get_parent(), n, nindex, nindex);
                    deallocate_node(allocator, n);
                    return { right, nindex };
                }
            }

        #if !defined(BTREE_VALUE_NODE_APPEND)
            assert(false);
            std::abort();
        #endif
            return { n, nindex };
        }
            
        template< typename Node > static std::tuple< node_descriptor< Node >, node_size_type > get_right(node_descriptor< Node > n, node_size_type index, bool recursive = false)
        {
            node_descriptor< internal_node* > p(n.get_parent());
            size_type depth = 1;
            while (p)
            {
                auto children = p.template get_children< Node >();
                if (index + 1 < children.size())
                {
                    if (depth == 1)
                    {
                        return { children[index + 1], index + 1 };
                    }
                    else
                    {
                        return { first_node< Node >(children[index + 1], depth), 0 };
                    }
                }
                else if (recursive)
                {
                    if (p.get_parent())
                    {
                        index = get_index(p);
                        ++depth;
                    }

                    p = p.get_parent();
                }
                else
                {
                    break;
                }
            }

            return { nullptr, 0 };
        }

    #if defined(BTREE_VALUE_NODE_LR)
        static std::tuple< node_descriptor< value_node* >, node_size_type > get_right(node_descriptor< value_node* > n, node_size_type index)
        {
            return { n.node()->right, n.node()->right ? get_index(desc(n.node()->right)) : 0 };
        }
    #endif

        template< typename Node > static std::tuple< node_descriptor< Node >, node_size_type > get_left(node_descriptor< Node > n, node_size_type index, bool recursive = false)
        {
            node_descriptor< internal_node* > p = n.get_parent();
            size_type depth = 1;
            while (p)
            {
                auto children = p.template get_children< Node >();
                if (index > 0)
                {
                    if (depth == 1)
                    {
                        return { children[index - 1], index - 1 };
                    }
                    else
                    {
                        return { last_node< Node >(children[index - 1], depth), 0 };
                    }
                }
                else if (recursive)
                {
                    if (p.get_parent())
                    {
                        index = get_index(p);
                        ++depth;
                    }

                    p = p.get_parent();
                }
                else
                {
                    break;
                }
            }

            return { nullptr, 0 };
        }

    #if defined(BTREE_VALUE_NODE_LR)
        static std::tuple< node_descriptor< value_node* >, node_size_type > get_left(node_descriptor< value_node* > n, node_size_type index)
        {
            return { n.node()->left, n.node()->left ? get_index(desc(n.node()->left)) : 0 };
        }
    #endif

        template< typename Node > static node_size_type get_index(node_descriptor< Node > n)
        {
            auto index = node_size_type();

            if (n && n.get_parent())
            {
                index = find_node_index(n.get_parent().template get_children< node* >(), n);
            }

            return index;
        }

    private:
        static void set_parent(bool valuenode, node* n, internal_node* parent)
        {
            if (valuenode)
            {
                desc(node_cast<value_node*>(n)).set_parent(parent);
            }
            else
            {
                desc(node_cast<internal_node*>(n)).set_parent(parent);
            }
        }

        template < typename Node > static Node node_cast(node* n)
        {
            assert(n);
        #if defined(_DEBUG)
            assert(dynamic_cast<Node>(n) == n);
        #endif
            return reinterpret_cast<Node>(n);
        }

        bool compare_lte(const key_type& lhs, const key_type& rhs) const
        {
            Compare compare;
            return compare(lhs, rhs) || !compare(rhs, lhs);
        }

        template < typename Node > static bool full(const node_descriptor< Node > n)
        {
            return n.get_keys().size() == n.get_keys().capacity();
        }

    protected:
    #if defined(BTREE_CHECK_TREE_INVARIANTS)
        // TODO: try to move this code to some outside class

        void checktree()
        {
            if (root_)
            {
                checktree(root_, nullptr, 1);

                // TODO: after we move last_node/first_node to pointer versions, we will need a way how to find them through traversal.
                assert(last_node_ == last_node< value_node* >(root_, depth_));
                assert(first_node_ == first_node< value_node* >(root_, depth_));

                // TODO:
                //assert(count == std::distance(begin(), end()));                    

                size_type count_forward = 0;
                for (auto it = begin(); it != end(); ++it)
                {
                    ++count_forward;
                }
                assert(count_forward == size());

                size_type count_backward = 0;
                auto it = end();
                do
                {
                    --it;
                    ++count_backward;
                } while (it != begin());
                assert(count_backward == size());

                size_type count_node = 0;
                for (auto vn = first_node_; vn != nullptr; )
                {
                    count_node += vn->size;

                    if (vn == first_node_)
                    {
                        assert(vn->left == nullptr);
                    }
                    else if (vn == last_node_)
                    {
                        assert(vn->right == nullptr);
                    }

                    if (vn->right)
                    {
                        assert(vn->right->left == vn);
                    }

                    vn = vn->right;
                }

                assert(count_node == size());
            }
            else
            {
                assert(empty());
                assert(size() == 0);
                assert(begin() == end());
                assert(first_node_ == nullptr);
                assert(last_node_ == nullptr);
            }
        }

        void checktree(node* n, internal_node* parent, node_size_type depth)
        {
            // TODO: assert element count in N-2N.
            if (depth_ == depth)
            {
                auto vn = node_cast<value_node*>(n);
                assert(vn->parent == parent);
            }
            else
            {
                auto in = node_cast<internal_node*>(n);
                assert(in->parent == parent);

                // Child/parent relationship check

                auto children = desc(in).template get_children< node* >();
                for (auto child : children)
                {
                    if (depth + 1 == depth_)
                    {
                        auto node = node_cast<value_node*>(child);
                        assert(node->parent == in);
                    }
                    else
                    {
                        auto node = node_cast<internal_node*>(child);
                        assert(node->parent == in);
                    }

                    checktree(child, in, depth + 1);
                }
            }
        }
    #endif

    private:
        // TODO: add SOO
        node* root_;
        value_node* first_node_;
        value_node* last_node_;

        size_type size_;
        size_type depth_;

        // TODO: stateful comparator not supported
    };

    template < typename Key, typename SizeType, SizeType Dim > struct internal_node : node
    {
        enum { N = Dim };
                
        alignas(Key) uint8_t keys[(2 * N - 1) * sizeof(Key)];
        alignas(node*) uint8_t children[2 * N * sizeof(node*)];
        internal_node< Key, SizeType, N >* parent;
        SizeType size;
    };

    template < typename Key, typename SizeType, SizeType N > struct node_descriptor< internal_node< Key, SizeType, N >* >
    {
        using internal_node_type = internal_node< Key, SizeType, N >;
        using size_type = SizeType;

        node_descriptor(internal_node_type* n)
            : node_(n)
        {}

        template < typename Allocator > void cleanup(Allocator& allocator) { get_keys().clear(allocator); }

        auto get_keys() { return fixed_vector< Key, keys_descriptor< internal_node_type*, size_type, 2 * N - 1 > >(node_); }
        auto get_keys() const { return fixed_vector< Key, keys_descriptor< internal_node_type*, size_type, 2 * N - 1 > >(node_); }

        template < typename Node > auto get_children() { return fixed_vector< Node, children_descriptor< internal_node_type*, size_type, 2 * N > >(node_); }

        node_descriptor< internal_node_type* > get_parent() { return node_->parent; }
        void set_parent(internal_node_type* p) { node_->parent = p; }

        operator internal_node_type* () { return node_; }
        internal_node_type* node() { return node_; }

    private:
        internal_node_type* node_;
    };

    template < typename Key, typename Value, typename SizeType, SizeType N, typename InternalNodeType > struct value_node;
}
