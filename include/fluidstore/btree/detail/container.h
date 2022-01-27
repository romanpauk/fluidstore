#pragma once

// TODO: what should happen with btree:
// 1. check exception-safety
// 2. check function signatures
// 3. check iterators
// 4. separate source control
// 5. copying
// 6. batch initialization
// 7. stable iterators
// 8. multimap/multiset
// 9. stateful/stateless comparator
// 10. stateful/stateless allocator

#include <memory>
#include <type_traits>
#include <algorithm>
#include <iterator>

#define BTREE_VALUE_NODE_LR
#define BTREE_VALUE_NODE_APPEND
//#define BTREE_INTERNAL_NODE_LR

#if defined(_DEBUG)
    //#define BTREE_CHECK_VECTOR_INVARIANTS
    #define BTREE_CHECK_TREE_INVARIANTS

    // TODO: try lambdas, but look at code generated for release.
    #define BTREE_CHECK(code) \
        do { \
            (code); \
            this->check_tree(); \
        } while(0)

    #define BTREE_CHECK_RETURN(code) \
        do { \
            auto result = (code); \
            this->check_tree(); \
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

    template < typename SizeType > struct array_size_copy
    {
        using size_type = SizeType;

        array_size_copy(size_type size): size_(size) {}

        void set_size(size_type size) { size_ = size; }
        size_type size() const { return size_; }

    private:
        size_type size_;
    };

    template < typename SizeType > struct array_size_ref
    {
        using size_type = SizeType;

        array_size_ref(size_type& size): size_(size) {}

        void set_size(size_type size) { size_ = size; }
        size_type size() const { return size_; }

    private:
        size_type& size_;
    };

    template < typename ArraySize, typename ArraySize::size_type Capacity > struct array_descriptor
        : ArraySize
    {
        using size_type = typename ArraySize::size_type;

        template < typename ArraySizeT > array_descriptor(void* data, ArraySizeT&& size)
            : ArraySize(std::forward< ArraySizeT >(size))
            , data_(data)
        {}

        size_type size() const { return ArraySize::size(); }
        constexpr size_type capacity() const { return Capacity; }

    #if defined(_DEBUG)
        void set_size(size_type size)
        {
            assert(size <= capacity());
            ArraySize::set_size(size);
        }
    #endif

        void* data() { return data_; }
        const void* data() const { return data_; }

    private:
        void* data_;
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

        template < typename Pair > static const Key& get_key(const Pair& p) { return p.first; }

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
        enum class direction
        {
            left,
            right
        };

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
                    size_type tmp;
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

        // TODO: test, add to set/map
        template < typename AllocatorT > size_type erase(AllocatorT& allocator, const key_type& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase(allocator, it);
                return 1;
            }

            return 0;
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
                        auto right = get_right(node);
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
                    auto n = rebalance_erase(allocator, depth_, node);
                    auto kindex = find_key_index(n.get_keys(), key);

                    --size_;
                    auto ndata = n.get_data();
                    if (ndata.erase(allocator, ndata.begin() + kindex) == ndata.end())
                    {
                        auto right = get_right(n);
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

                // TODO: this should forward but can't because of fixed_split_vector
                return insert(allocator, root, 0, std::move(value));
            }

            // TODO: look at key extraction
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
                    // TODO: reusing nindex here makes some difference, it seems.
                    n = rebalance_insert(allocator, depth_, n, nindex, key);
                    return insert(allocator, n, std::move(value));
                }
                else
                {
                    n = rebalance_insert(allocator, depth_, n, key);
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

            if (hint == end())
            {
                auto n = desc(hint.node_);
                
                auto keys = n.get_keys();
                const auto key = value_type_traits_type::get_key(value);
                if (compare(keys[hint.kindex_ - 1], key))
                {
                    if (!full(n))
                    {
                        return insert(allocator, n, hint.kindex_, std::move(value));
                    }
                    else
                    {
                        n = rebalance_insert(allocator, depth_, n, key);
                        return insert(allocator, n, std::move(value));
                    }
                }
            }

            assert(false);
            return emplace(allocator, std::move(value));
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
                    nindex = static_cast<node_size_type>(kindex + (key == nkeys[kindex]));
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
                // TODO: investigate why this differs.
            #if defined(BTREE_VALUE_NODE_APPEND)
                // Either next node, or end.
                auto right = get_right(vn);
                return right ? iterator(right, 0) : end();
            #else
                return end();
            #endif
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
            // Counts number of elements smaller than key
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

        template< typename AllocatorT, typename Node > void remove_node(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex, node_size_type kindex)
        {            
            auto p = n.get_parent();
            auto pchildren = p.template get_children< node* >();
            assert(!pchildren.empty());
            pchildren.erase(allocator, pchildren.begin() + nindex);
                        
            auto pkeys = p.get_keys();           
            pkeys.erase(allocator, pkeys.begin() + kindex);

            if (pkeys.empty())
            {
                assert(pchildren.size() == 1);

                auto root = root_;
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

        // TODO: check that returned reference is valid where split_key is used.
        static const Key& split_key(node_descriptor< internal_node* > n)
        {
            assert(full(n));
            return *(n.get_keys().begin() + internal_node::N);
        }

        template < typename Node > void link_node(node_descriptor< Node > lnode, node_descriptor< Node > rnode)
        {
            auto znode = lnode.node()->right;
            lnode.node()->right = rnode;
            rnode.node()->right = znode;
            rnode.node()->left = lnode;
            if (znode)
            {
                znode->left = rnode;
            }
        }
        
        template < typename Node > void unlink_node(node_descriptor< Node > n)
        {
            auto left = n.node()->left;
            auto right = n.node()->right;
            
            if (left)
            {
                left->right = right;
            }

            if (right)
            {
                right->left = left;
            }
        }

        template < typename AllocatorT > std::tuple< node_descriptor< internal_node* >, const key_type > split_node(AllocatorT& allocator, size_type depth, node_descriptor< internal_node* > lnode, const key_type&)
        {
            assert(full(lnode));
            auto rnode = desc(allocate_node< internal_node >(allocator));

            auto lchildren = lnode.template get_children< node* >();
            auto rchildren = rnode.template get_children< node* >();

            assert(depth_ >= depth + 1);
            rchildren.insert(allocator, rchildren.begin(), lchildren.begin() + internal_node::N, lchildren.end());
            std::for_each(rchildren.begin(), rchildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, rnode); });

            auto lkeys = lnode.get_keys();
            auto rkeys = rnode.get_keys();

            auto begin = lkeys.begin() + internal_node::N;
            rkeys.insert(allocator, rkeys.end(), std::make_move_iterator(begin), std::make_move_iterator(lkeys.end()));

            Key splitkey = std::move(*(begin - 1));

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(allocator, begin - 1, lkeys.end());

            assert(lkeys.size() == internal_node::N - 1);
            assert(lnode.template get_children< node* >().size() == internal_node::N);
            assert(rkeys.size() == internal_node::N - 1);
            assert(rnode.template get_children< node* >().size() == internal_node::N);
            assert(lkeys.back() < splitkey);
            assert(rkeys.front() >= splitkey);

        #if defined(BTREE_INTERNAL_NODE_LR)
            link_node(lnode, rnode);
        #endif

            return { rnode, splitkey };
        }
                
        template < typename AllocatorT > std::tuple< node_descriptor< value_node* >, const key_type& > split_node(AllocatorT& allocator, size_type, node_descriptor< value_node* > lnode, const key_type& splitkey)
        {
            assert(full(lnode));
            auto rnode = desc(allocate_node< value_node >(allocator));
            
            auto lkeys = lnode.get_keys();
            auto rkeys = rnode.get_keys();            
                        
        #if defined(BTREE_VALUE_NODE_APPEND)
            // In case of appending to the right-most node, we will allow rnode to be empty as there might be more appends comming.
            // The tree will be very slightly disbalanced on its right edge but that is ok, it does not impact the overall complexity.
            // The node needs to be properly split and items distributed if there is a right node or the operation is not an append.
            // TODO: compare
            auto right = get_right(lnode);
            if (right || !compare(lkeys.back(), splitkey))
            {
        #endif
                auto ldata = lnode.get_data();
                auto rdata = rnode.get_data();

                auto begin = ldata.begin() + value_node::N;
                rdata.insert(allocator, rdata.end(), std::make_move_iterator(begin), std::make_move_iterator(ldata.end()));

                // Keep splitkey.
                ldata.erase(allocator, begin, ldata.end());
                assert(ldata.size() == value_node::N);
                assert(rdata.size() == value_node::N);
                assert(lkeys.back() < rkeys.front());
        #if defined(BTREE_VALUE_NODE_APPEND)
            }
            else
            {
                assert(full(lnode));
                assert(rkeys.empty());
                assert(lkeys.back() < splitkey);
            }
        #endif

        #if defined(BTREE_VALUE_NODE_LR)
            link_node(lnode, rnode);
        #endif

        #if defined(BTREE_VALUE_NODE_APPEND)
            // In case of append, there is nothing in rkeys yet. But the separator is clear as that is the key that caused a split.
            return { rnode, rkeys.empty() ? splitkey : rkeys.front() };
        #else
            return { rnode, rkeys.front() };
        #endif
        }

        template < typename AllocatorT > void free_node(AllocatorT& allocator, node* n, size_type depth)
        {
            // TODO: can throw, will leave depth/size in mess
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

        // TODO: make part of verify_.
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

        template < typename Node > size_type get_level(node_descriptor< Node* > n)
        {
            size_type level = 0;
            auto parent = n.get_parent();
            while (parent)
            {
                ++level;
                parent = parent.get_parent();
            }

            return level;
        }
        template < typename AllocatorT > bool share_keys(AllocatorT& allocator, direction dir, size_t, node_descriptor< value_node* > target, node_descriptor< value_node* > source)
        {
            if (!source)
            {
                return false;
            }

            assert(source.get_parent() == target.get_parent());
            assert(target.get_keys().size() <= target.get_keys().capacity() / 2);

            auto sdata = source.get_data();            
            auto tdata = target.get_data();

        #if defined(_DEBUG)    
            auto tkeys = target.get_keys();
            auto skeys = source.get_keys();
        #endif

            if (sdata.size() > value_node::N && tdata.size() < 2 * value_node::N)
            {
                // TODO: skeys, pkeys needed only for asserts.

                auto pkeys = source.get_parent().get_keys();

                if (dir == direction::right)
                {
                    // Right-most key from the left node
                    auto borrow = sdata.end() - 1;
                    auto pkindex = find_key_index(pkeys, value_type_traits_type::get_key(*borrow));

                    tdata.emplace(allocator, tdata.begin(), std::move(*borrow));
                    sdata.erase(allocator, borrow);

                    auto separator = tdata.begin();
                    pkeys[pkindex] = value_type_traits_type::get_key(*separator);
                    assert(check_separator(pkeys, skeys.back(), pkeys[pkindex], tkeys.front()));
                }
                else
                {
                    // Left-most key from the right node
                    auto borrow = sdata.begin();
                    auto pkindex = find_key_index(pkeys, value_type_traits_type::get_key(*(tdata.end() - 1)));

                    tdata.emplace(allocator, tdata.end(), std::move(*borrow));
                    sdata.erase(allocator, borrow);

                    auto separator = sdata.begin();
                    pkeys[pkindex] = value_type_traits_type::get_key(*separator);
                    assert(check_separator(pkeys, tkeys.back(), pkeys[pkindex], skeys.front()));
                }

            #if defined(BTREE_VALUE_NODE_APPEND)
                if (target.node() != last_node_)
                {
            #endif
                    // TODO: we are adding one element to target node as it is unbalanced so we can erase. But it is rightmost node, it will be unbalanced even after addition.
                    // The proper way to do is to remove the node instead.
                    assert(target.get_keys().size() > target.get_keys().capacity() / 2);
            #if defined(BTREE_VALUE_NODE_APPEND)
                }
            #endif
            
                return true;
            }

        #if defined(BTREE_VALUE_NODE_APPEND)
            if (source.node() == last_node_ && !sdata.empty())
            {    
                auto pkeys = source.get_parent().get_keys();

                // Left-most key from the right node
                auto borrow = sdata.begin();
                auto pkindex = find_key_index(pkeys, value_type_traits_type::get_key(*(tdata.end() - 1)));

                tdata.emplace(allocator, tdata.end(), std::move(*borrow));
                sdata.erase(allocator, borrow);
                                
                if (!sdata.empty())
                {                    
                    auto separator = sdata.begin();
                    pkeys[pkindex] = value_type_traits_type::get_key(*separator);
                    assert(check_separator(pkeys, tkeys.back(), pkeys[pkindex], skeys.front()));
                }
                else
                {
                    // Node will be removed in the caller, no need to update the pkeys.
                }
                
                assert(target.get_keys().size() > target.get_keys().capacity() / 2);

                return true;
            }
        #endif

            return false;
        }

        template < typename AllocatorT > bool share_keys(AllocatorT& allocator, direction dir, size_type depth, node_descriptor< internal_node* > target, node_descriptor< internal_node* > source)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source.get_keys();
            auto tkeys = target.get_keys();
                       
            assert(depth_ >= depth + 1);
            assert(source.get_parent() == target.get_parent());
            assert(target.get_keys().size() <= target.get_keys().capacity() / 2);

            if (skeys.size() > internal_node::N - 1 && tkeys.size() < 2 * internal_node::N - 1)
            {
                auto p = source.get_parent();
                auto pkeys = p.get_keys();
                                       
                auto schildren = source.template get_children< node* >();
                auto tchildren = target.template get_children< node* >();

                if (dir == direction::right)
                {
                    auto pkindex = find_key_index(pkeys, skeys.back());
                    
                    tchildren.emplace(allocator, tchildren.begin(), schildren[skeys.size()]);
                    set_parent(depth_ == depth + 1, schildren[skeys.size()], target);
                                       
                    tkeys.emplace(allocator, tkeys.begin(), std::move(pkeys[pkindex]));

                    pkeys[pkindex] = std::move(*(skeys.end() - 1));
                    skeys.erase(allocator, skeys.end() - 1);

                    assert(check_separator(pkeys, skeys.back(), pkeys[pkindex], tkeys.front()));
                }
                else
                {
                    auto pkindex = find_key_index(pkeys, tkeys.back());

                    tkeys.emplace(allocator, tkeys.end(), std::move(pkeys[pkindex]));
                    auto child = schildren[0];
                    set_parent(depth_ == depth + 1, child, target);
                                                            
                    schildren.erase(allocator, schildren.begin());
                    tchildren.emplace(allocator, tchildren.end(), child);

                    pkeys[pkindex] = std::move(*skeys.begin());
                    skeys.erase(allocator, skeys.begin());

                    assert(check_separator(pkeys, tkeys.back(), pkeys[pkindex], skeys.front()));
                }
                        
                assert(target.get_keys().size() > target.get_keys().capacity() / 2);

                return true;
            }

            return false;
        }

        template < typename AllocatorT > bool merge_keys(AllocatorT& allocator, direction dir, size_type depth, node_descriptor< value_node* > target, node_descriptor< value_node* > source)
        {
            if (!target)
            {
                return false;
            }
                        
            assert(source.get_parent() == target.get_parent());
            
            auto tdata = target.get_data();
            if (tdata.size() == value_node::N)
            {
                auto sdata = source.get_data();
                tdata.insert(allocator, dir == direction::left ? tdata.end() : tdata.begin(), std::make_move_iterator(sdata.begin()), std::make_move_iterator(sdata.end()));

                if (dir == direction::left)
                {                  
                #if defined(BTREE_VALUE_NODE_LR)
                    unlink_node(source);
                #endif

                    if (last_node_ == source)
                    {
                        last_node_ = target;
                    }
                }
                else
                {
                #if defined(BTREE_VALUE_NODE_LR)
                    unlink_node(source);
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

        template < typename AllocatorT > bool merge_keys(AllocatorT& allocator, direction dir, size_type depth, node_descriptor< internal_node* > target, node_descriptor< internal_node* > source)
        {
            if (!target)
            {
                return false;
            }
                        
            assert(source.get_parent() == target.get_parent());
                                    
            auto skeys = source.get_keys();
            auto tkeys = target.get_keys();

            if (tkeys.size() == internal_node::N - 1)
            {                
                auto p = source.get_parent();
                auto pkeys = p.get_keys();             
               
                assert(depth_ >= depth + 1);

                auto schildren = source.template get_children< node* >();
                auto tchildren = target.template get_children< node* >();

                if (dir == direction::left)
                {                  
                    auto pkindex = find_key_index(pkeys, tkeys.back());
                    
                    tchildren.insert(allocator, tchildren.end(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator, tkeys.end(), pkeys[pkindex]);
                    tkeys.insert(allocator, tkeys.end(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                }
                else
                {                   
                    auto pkindex = find_key_index(pkeys, skeys.back());
                    
                    tchildren.insert(allocator, tchildren.begin(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator, tkeys.begin(), pkeys[pkindex]);
                    tkeys.insert(allocator, tkeys.begin(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));                    
                }

                return true;
            }

            return false;
        }

        template < typename AllocatorT, typename Node > node_descriptor< Node* > rebalance_insert(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, const Key& key)
        {
            assert(full(n));
            if (n.get_parent())
            {
                return rebalance_insert(allocator, depth, n, get_index(n), key);
            }
            else
            {
                auto root = desc(allocate_node< internal_node >(allocator));
                auto [p, splitkey] = split_node(allocator, depth, n, key);
                
                auto children = root.template get_children< node* >();
                children.emplace_back(allocator, n);
                n.set_parent(root);
                children.emplace_back(allocator, p);
                p.set_parent(root);
                              
                Compare compare;
                auto cmp = !compare(key, splitkey);

            #if defined(BTREE_VALUE_NODE_APPEND)
                if (p.node()->size) 
                {
            #endif
                    assert(check_separator(root.get_keys(), n.get_keys().back(), splitkey, p.get_keys().front()));
            #if defined(BTREE_VALUE_NODE_APPEND)
                }
            #endif

                root.get_keys().emplace_back(allocator, std::move(splitkey));
                
                if (root_ == last_node_)
                {
                    last_node_ = node_cast<value_node*>(p);
                    first_node_ = node_cast<value_node*>(n);
                }               

                root_ = root;
                ++depth_;
            
                return cmp ? p : n;
            }
        }

        template < typename AllocatorT, typename Node > node_descriptor< Node* > rebalance_insert(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex, const Key& key)
        {
            assert(full(n));
            assert(n.get_parent());
                        
            if (full(n.get_parent()))
            {
                assert(depth > 1);                
                depth_check< true > dc(depth_, depth);
                rebalance_insert(allocator, depth - 1, n.get_parent(), split_key(n.get_parent()));
                
                // Parent may changed, so fetch n's index again
                nindex = get_index(n);
            }
                        
            // TODO: this only splits, but it can also share to get rid of some keys.
            auto [p, splitkey] = split_node(allocator, depth, n, key);
            
            auto pchildren = n.get_parent().template get_children< node* >();
            pchildren.emplace(allocator, pchildren.begin() + nindex + 1, p);
            p.set_parent(n.get_parent());

            auto pkeys = n.get_parent().get_keys();
            pkeys.emplace(allocator, pkeys.begin() + nindex, splitkey);

        #if defined(BTREE_VALUE_NODE_APPEND)
            if (p.node()->size)
            {
            #endif
                assert(check_separator(pkeys, n.get_keys().back(), splitkey, p.get_keys().front()));
        #if defined(BTREE_VALUE_NODE_APPEND)
            }
        #endif

            // TODO: where does this belong?
            if constexpr (std::is_same_v< Node, value_node >)
            {
                auto vn = node_cast<value_node*>(n.node());
                if (last_node_ == vn)
                {
                    last_node_ = node_cast<value_node*>(p.node());
                }
            }
                        
            auto cmp = !compare(key, splitkey);
            return cmp ? p : n;
        }

        template < typename AllocatorT, typename Node > node_descriptor< Node* > rebalance_erase(AllocatorT& allocator, size_type depth, node_descriptor< Node* > n)
        {            
            if (n.get_parent())
            {              
                // Rebalance parent right away as in 3 out of 4 cases we might need to rebalance it anyway.
                // TODO: unnecessary in all cases...
                auto pkeys = n.get_parent().get_keys();
                if (pkeys.size() <= pkeys.capacity() / 2)
                {
                    depth_check< false > dc(depth_, depth);
                    rebalance_erase(allocator, depth - 1, n.get_parent());                    
                }
                                
                auto nindex = get_index(n);
                auto [left, lindex] = get_left(n, nindex, false);
                if (left && share_keys(allocator, direction::right, depth, n, left))
                {
                    return n;
                }
 
                auto [right, rindex] = get_right(n, nindex, false);
                if (right && share_keys(allocator, direction::left, depth, n, right))
                {
                #if defined(BTREE_VALUE_NODE_APPEND)
                    // TODO: investigate - right was 0, so possibly rigthtmost node append optimization?
                    if (right.get_keys().empty())
                    {
                        if constexpr (std::is_same_v < value_node, Node >)
                        {
                            auto vn = node_cast<value_node*>(n.node());
                            last_node_ = vn;
                        #if defined(BTREE_VALUE_NODE_LR)
                            vn->right = nullptr;
                        #endif
                        }                                      
                        
                        remove_node(allocator, depth, right, rindex, nindex);
                        deallocate_node(allocator, right);
                    }
                #endif
                    return n;
                }
                
                if (merge_keys(allocator, direction::left, depth, left, n))
                {       
                    remove_node(allocator, depth, n, nindex, nindex - 1);
                    deallocate_node(allocator, n);
                    return left;
                }
            
                std::tie(right, rindex) = get_right(n, nindex, false);
                if (merge_keys(allocator, direction::right, depth, right, n))
                {
                    remove_node(allocator, depth, n, nindex, nindex);
                    deallocate_node(allocator, n);
                    return right;
                }
            
            #if !defined(BTREE_VALUE_NODE_APPEND)
                assert(false);
                std::abort();
            #endif
                return n;
            }

            return nullptr;
        }
            
        // TODO: hide in #else, or remove (keep for verify_).
        template< typename Node > static node_descriptor< Node > get_right(node_descriptor< Node > n)
        {
            return std::get< 0 >(get_right(n, get_index(n)));
        }

        template< typename Node > static std::tuple< node_descriptor< Node >, node_size_type > get_right(node_descriptor< Node > n, node_size_type index, bool recursive = false)
        {
            node_descriptor< internal_node* > p(n.get_parent());
            std::tuple< node_descriptor< Node >, node_size_type > result(nullptr, 0);
            node_size_type level(1);

            while (p)
            {
                auto children = p.template get_children< Node >();
                if (index + 1 < children.size())
                {
                    result = { children[index + 1], index + 1 };
                    break;
                }
                else if(recursive)
                {
                    index = get_index(p);
                    p = p.get_parent();                    
                    ++level;
                }
                else
                {
                    break;
                }
            }

            if (recursive && std::get< 0 >(result) && level > 1)
            {
                return { first_node< Node >(std::get< 0 >(result), level), 0 };
            }
            else
            {
                return result;
            }
        }

    #if defined(BTREE_VALUE_NODE_LR)
        static std::tuple< node_descriptor< value_node* >, node_size_type > get_right(node_descriptor< value_node* > n, node_size_type index)
        {
            return { n.node()->right, n.node()->right ? index + 1 : 0 };
        }

        static node_descriptor< value_node* > get_right(node_descriptor< value_node* > n)
        {
            return n.node()->right;
        }
    #endif

    #if defined(BTREE_INTERNAL_NODE_LR)
        static std::tuple< node_descriptor< internal_node* >, node_size_type > get_right(node_descriptor< internal_node* > n, node_size_type index)
        {
            return { n.node()->right, n.node()->right ? index + 1 : 0 };
        }
    #endif

        template< typename Node > static std::tuple< node_descriptor< Node >, node_size_type > get_left(node_descriptor< Node > n, node_size_type index, bool recursive = false)
        {
            node_descriptor< internal_node* > p(n.get_parent());
            std::tuple< node_descriptor< Node >, node_size_type > result(nullptr, 0);
            node_size_type level(1);

            while (p)
            {
                auto children = p.template get_children< Node >();
                if (index > 0)
                {
                    result = { children[index - 1], index - 1 };
                    break;
                }
                else if(recursive)
                {
                    index = get_index(p);
                    p = p.get_parent();                    
                    ++level;
                }
                else
                {
                    break;
                }
            }

            if (recursive && std::get< 0 >(result) && level > 1)
            {
                auto node = last_node< Node >(std::get< 0 >(result), level);

                // TODO: last node, can be get from parent's children
                return { node, get_index(desc(node)) };
            }
            else
            {
                return result;
            }
        }

    #if defined(BTREE_VALUE_NODE_LR)
        static std::tuple< node_descriptor< value_node* >, node_size_type > get_left(node_descriptor< value_node* > n, node_size_type index)
        {            
            return { n.node()->left, n.node()->left ? index - 1 : 0 };
        }
    #endif

    #if defined(BTREE_INTERNAL_NODE_LR)
        static std::tuple< node_descriptor< internal_node* >, node_size_type > get_left(node_descriptor< internal_node* > n, node_size_type index)
        {
            return { n.node()->left, n.node()->left ? index - 1 : 0 };
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
        // TODO: parent is needed to find index of node, and that is needed as internal_nodes do not have left/right.
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

        bool compare(const key_type& lhs, const key_type& rhs) const
        {
            Compare cmp;
            return cmp(lhs, rhs);
        }

        template < typename Node > static bool full(const node_descriptor< Node > n)
        {
            return n.get_keys().size() == n.get_keys().capacity();
        }

    protected:
    #if defined(BTREE_CHECK_TREE_INVARIANTS)
        // TODO: try to move this code to some outside class

        void check_tree()
        {
            if (root_)
            {
                check_node(root_, nullptr, 1);

                // TODO: after we move last_node/first_node to pointer versions, we will need a way how to find them through traversal.
                assert(last_node_ == last_node< value_node* >(root_, depth_));
                assert(first_node_ == first_node< value_node* >(root_, depth_));
                                
                assert(std::distance(begin(), end()) == size());                    

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

            #if defined(BTREE_VALUE_NODE_LR)
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
            #endif

                check_levels(root_, 1);
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

        void check_node(node* n, internal_node* parent, node_size_type depth)
        {
            // TODO: assert element count in N-2N.
            if (depth_ == depth)
            {
                auto vn = node_cast<value_node*>(n);
                assert(vn->parent == parent);
                
                // Check that node is balanced
                if (vn != root_ 
                #if defined(BTREE_VALUE_NODE_APPEND)
                    && vn != last_node_
                #endif
                )
                {
                    assert(value_node::N <= vn->size && vn->size <= 2 * value_node::N);
                }
            }
            else
            {
                auto in = node_cast<internal_node*>(n);
                assert(in->parent == parent);

                auto keys = desc(in).get_keys();

                // Check that node is balanced
                if(in != root_)
                {
                    assert(internal_node::N - 1 <= in->size && in->size <= 2 * internal_node::N - 1);
                }

                // Check keys
                for (node_size_type i = 0; i < keys.size(); ++i)
                {
                    if (depth_ == depth + 1)
                    {
                        auto child = desc(in).template get_children< value_node* >()[i];
                        auto childkeys = desc(child).get_keys();
                        for (auto childkey : childkeys)
                        {
                            assert(childkey < keys[i]);
                        }
                    }
                    else
                    {
                        auto child = desc(in).template get_children< internal_node* >()[i];
                        auto childkeys = desc(child).get_keys();
                        for (auto childkey : childkeys)
                        {
                            assert(childkey < keys[i]);
                        }
                    }
                }

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

                    check_node(child, in, depth + 1);
                }               
            }
        }

        template < typename Node > void check_levels(Node* n, size_type depth)
        {
            if (depth == depth_)
            {
                auto vn = desc(node_cast<value_node*>(n));
                check_level(vn);
            }
            else
            {
                auto in = desc(node_cast<internal_node*>(n));
                check_level(in);
                check_levels(in.get_children< node* >()[0], depth + 1);
            }
        }

        template < typename Node > bool check_level(node_descriptor< Node > n)
        {
            std::vector< key_type > vec;
            auto level = get_level(n);
            while (n)
            {
                assert(get_level(n) == level);

                // TODO: why does this not compile?
                // vec.emplace(vec.end(), n.get_keys().begin(), n.get_keys().end());
                for (auto key : n.get_keys())
                {
                    vec.push_back(key);
                }

                assert(std::is_sorted(vec.begin(), vec.end()));

                n = std::get< 0 >(get_right(n, get_index(n), true));
            }

            return true;
        }

        template< typename Keys, typename Key > bool check_separator(const Keys& keys, const Key& left, const Key& separator, const Key& right)
        {
            assert(left < separator);
            assert(separator <= right);

            return true;
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
    #if defined(BTREE_INTERNAL_NODE_LR)
        internal_node< Key, SizeType, N >* left;
        internal_node< Key, SizeType, N >* right;
    #endif
        SizeType size;
    };

    template < typename Key, typename SizeType, SizeType N > struct node_descriptor< internal_node< Key, SizeType, N >* >
    {
        using internal_node_type = internal_node< Key, SizeType, N >;
        using size_type = SizeType;

        using key_array_descriptor = array_descriptor< array_size_ref< size_type >, 2 * N - 1 >;
        using children_array_descriptor = array_descriptor< array_size_copy< size_type >, 2 * N >;

        node_descriptor(internal_node_type* n)
            : node_(n)
        {}

        template < typename Allocator > void cleanup(Allocator& allocator) { get_keys().clear(allocator); }
                
        auto get_keys() { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }
        auto get_keys() const { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }
                
        template < typename Node > auto get_children() { return fixed_vector< Node, children_array_descriptor >(node_->children, node_->size ? node_->size + 1 : 0); }
                
        node_descriptor< internal_node_type* > get_parent() { return node_->parent; }
        void set_parent(internal_node_type* p) { node_->parent = p; }

        operator internal_node_type* () { return node_; }
        internal_node_type* node() { return node_; }

    private:
        internal_node_type* node_;
    };

    template < typename Key, typename Value, typename SizeType, SizeType N, typename InternalNodeType > struct value_node;
}
