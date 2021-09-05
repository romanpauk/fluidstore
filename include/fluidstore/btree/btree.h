#pragma once

#include <set>
#include <memory>
#include <type_traits>
#include <algorithm>

namespace btree
{
    template < typename T, typename Descriptor > struct fixed_vector
    {
    public:
        using size_type = typename Descriptor::size_type;

        //fixed_vector(const fixed_vector< T, Descriptor >&) = delete;
        //fixed_vector(fixed_vector< T, Descriptor >&&) = delete;

        fixed_vector(Descriptor desc) 
            : desc_(desc)
        {
            checkvec();
        }

        fixed_vector(Descriptor desc, size_type size)
            : desc_(desc)
        {
            desc_.set_size(size);
            checkvec();
        }

        template < typename Allocator, typename Ty > void emplace_back(Allocator& alloc, Ty&& value)
        {
            emplace(alloc, end(), std::forward< Ty >(value));
        }

        template < typename Allocator > void erase(Allocator& alloc, const T* index)
        {
            assert(begin() <= index && index < end());
            
            auto dest = std::move(const_cast<T*>(index) + 1, end(), const_cast<T*>(index));
            destroy(alloc, end()-1, end());
            
            desc_.set_size(size() - 1);

            checkvec();
        }

        template < typename Allocator > void erase(Allocator& alloc , T* from, T* to)
        {
            assert(begin() <= from && from <= to);
            assert(from <= to && to <= end());

            if (to == end())
            {
                destroy(alloc, from, to);
                desc_.set_size(size() - (to - from));
            }
            else
            {
                std::abort(); // TODO
            }

            checkvec();
        }

        size_type size() const { return desc_.size(); }
        size_type capacity() const { return desc_.capacity(); }
        bool empty() const { return desc_.size() == 0; }

        template < typename Allocator > void clear(Allocator& alloc)
        {
            destroy(alloc, begin(), end());
            desc_.set_size(0);
        }

        T* begin() { return reinterpret_cast< T* >(desc_.data()); }
        const T* begin() const { return reinterpret_cast< const T* >(desc_.data()); }

        T* end() { return reinterpret_cast< T* >(desc_.data()) + desc_.size(); }
        const T* end() const { return reinterpret_cast< const T* >(desc_.data()) + desc_.size(); }

        template < typename Allocator, typename Ty > void emplace(Allocator& alloc, const T* it, Ty&& value)
        {
            assert(size() < capacity());
            assert(it >= begin());
            assert(it <= end());

            if (it < end())
            {
                move_backward(alloc, const_cast<T*>(it), end(), end() + 1);
             
                //const_cast< T& >(*it) = T(std::forward< Ty >(value));
                std::allocator_traits< Allocator >::destroy(alloc, const_cast<T*>(it));
                std::allocator_traits< Allocator >::construct(alloc, const_cast<T*>(it), std::forward< Ty >(value));
            }
            else
            {
                std::allocator_traits< Allocator >::construct(alloc, const_cast<T*>(it), std::forward< Ty >(value));
            }

            desc_.set_size(desc_.size() + 1);

            checkvec();
        }
        
        template < typename Allocator, typename U > void insert(Allocator& alloc, T* it, U from, U to)
        {
            assert(begin() <= it && it <= end());
            assert((uintptr_t)(to - from + it - begin()) <= capacity());
            
            if (it < end())
            {
                move_backward(alloc, const_cast<T*>(it), end(), end() + (to - from));
            }
            
            copy(alloc, from, to, it);
            desc_.set_size(size() + to - from);

            checkvec();
        }

        T& operator[](size_type index)
        {
            assert(index < size());
            return *(begin() + index);
        }

        const T& operator[](size_type index) const 
        {
            assert(index < size());
            return *(begin() + index);
        }

    private:
        template < typename Allocator > void destroy(Allocator& alloc, T* first, T* last)
        {
            assert(last > first);
            if constexpr (!std::is_trivially_destructible_v< T >)
            {
                while (first != last)
                {
                    std::allocator_traits< Allocator >::destroy(alloc, first++);
                }
            }
        }
        
        template < typename Allocator > void move_backward(Allocator& alloc, T* first, T* last, T* dest)
        {
            assert(last > first);
            assert(dest > last);

            if constexpr (std::is_trivially_copyable_v< T >)
            {
                std::move_backward(first, last, dest);
            }
            else
            {
                if (dest > end())
                {
                    size_type uninitialized_count = std::min(last - first, dest - end());
                    while (uninitialized_count--)
                    {
                        std::allocator_traits< Allocator >::construct(alloc, --dest, std::move(*--last));
                    }
                }

                std::move_backward(first, last, dest);
            }
        }

        template < typename Allocator, typename U > void copy(Allocator& alloc, U first, U last, T* dest)
        {
            assert(last > first);

            if constexpr (std::is_trivially_copyable_v< T >)
            {
                std::copy(first, last, dest);
            }
            else
            {
                size_type cnt = 0;
                if (dest < end())
                {
                    cnt = std::min(last - first, end() - dest);
                    std::copy(first, first + cnt, dest);
                }

                std::uninitialized_copy(first + cnt, last, dest + cnt);
            }
        }

        void checkvec()
        {
        #if defined(_DEBUG)
            assert(end() - begin() == size());
            assert(size() <= capacity());

            // vec_.assign(begin(), end());
        /*
            if (vec_.size() > 1)
            {
                for (size_type i = 0; i < size() - 1; ++i)
                {
                    if (vec_[i] >= vec_[i + 1])
                    {
                        assert(false);
                    }
                }
            }
        */
        #endif
        }

        Descriptor desc_;

    #if defined(_DEBUG)
        std::vector< T > vec_;
    #endif
    };

    template < typename Key, typename Compare = std::less< Key >, typename Allocator = std::allocator< Key > > class set
    {
        using node_size_type = uint8_t;
        static const auto _N = std::max(std::size_t(8), std::size_t(64 / sizeof(Key))) / 2;
        static_assert((1ull << sizeof(node_size_type) * 8) > 2 * _N);

    public:        
        static const node_size_type N = _N;

        using size_type = size_t;
        using value_type = Key;
        using allocator_type = Allocator;

        // Some ideas about the layout:
        // 
        // Memory and file-mapped cases are effectively the same, with the difference that file mapped needs to use offsets instead of pointers
        // and that it cannot store anything into pointers.
        // 
        // On a way down because of searching, just key and child pointers (and values at the end) are accessed.
        //      = align keys, child pointers and values on cache line
        // On a way down because if insert/erase, it is possible that we will need to go back up to rebalance.
        //      = the parent pointer is on another cache line and might not be loaded
        // 
        // File-mapped case:
        //
        // Memory case:
        //      value_node
        //          N keys
        //         (N values)
        //          parent, count
        //          
        //      internal_node
        //          N-1 keys (actually N, but there will be 1 unused - and that should be used for count)
        //          N pointers
        //          parent
        //
        // - ideally, both keys and pointers will be cache-aligned
        //      int8    64
        //      int16   32
        //      int32   16
        //      int64   8

        // This class will not have any data, it is there just to have common pointer to both node types.
        struct node
        {
        protected:
            node() {}
        #if defined(_DEBUG)
            virtual ~node() {}
        #endif
        };

        struct internal_node : node
        {
            internal_node()
                : size()
                , parent()
            {}

            template < typename Allocator > void cleanup(Allocator& allocator)
            {
                get_keys().clear(allocator);
            }

            std::tuple< internal_node*, node_size_type > get_left(node_size_type index, bool recursive = false) 
            { 
                return set< Key, Compare, Allocator >::get_left< internal_node >(parent, index, recursive); 
            }

            std::tuple< internal_node*, node_size_type > get_right(node_size_type index, bool recursive = false) 
            { 
                return set< Key, Compare, Allocator >::get_right< internal_node >(parent, index, recursive); 
            }

            auto get_keys() { return fixed_vector< Key, internal_keys >(this); }
            //const auto get_keys() const { return fixed_vector< Key, internal_keys >(this); }

            template < typename Node > auto get_children() { return fixed_vector< Node, internal_children >(this); }

            internal_node* get_parent() { return parent; }
            void set_parent(internal_node* n) { parent = n; }
            
            bool full() //const
            {
                auto keys = get_keys();
                return keys.size() == keys.capacity();
            }

            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            uint8_t children[2 * N * sizeof(node*)];
            node_size_type size; 
            internal_node* parent;
        };

        struct value_node : node
        {
            value_node()
                : size()
                , parent()
            {}

            template < typename Allocator > void cleanup(Allocator& allocator)
            {
                get_keys().clear(allocator);
            }

            std::tuple< value_node*, node_size_type > get_left(node_size_type index, bool recursive = false) 
            { 
                return set< Key, Compare, Allocator >::get_left< value_node >(parent, index, recursive); 
            }
            
            std::tuple< value_node*, node_size_type > get_right(node_size_type index, bool recursive = false) 
            { 
                return set< Key, Compare, Allocator >::get_right< value_node >(parent, index, recursive); 
            }
            
            auto get_keys() { return fixed_vector< Key, value_keys >(this); }
            //auto get_keys() const { return fixed_vector< Key, value_keys >(this); }

            internal_node* get_parent() { return parent; }
            void set_parent(internal_node* n) { parent = n; }
            
            bool full() //const
            {
                auto keys = get_keys();
                return keys.size() == keys.capacity();
            }

            uint8_t keys[2 * N * sizeof(Key)];
            // uint8_t values[2 * N * sizeof(Value)];
            node_size_type size; // TODO: this needs to be smaller, and elsewhere.
            internal_node* parent;
        };

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

        template < typename Node, node_size_type Capacity > struct keys_descriptor
        {
            using size_type = node_size_type;

            keys_descriptor(Node* node)
                : node_(node)
            {}

            size_type size() const { return node_->size; }
            size_type capacity() const { return Capacity; }

            void set_size(size_type size)
            {
                assert(size <= capacity());
                node_->size = size;
            }

            Key* data() { return reinterpret_cast<Key*>(node_->keys); }
            const Key* data() const { return reinterpret_cast<const Key*>(node_->keys); }

        private:
            Node* node_;
        };

        using internal_keys = keys_descriptor< internal_node, 2 * N - 1 >;
        using value_keys = keys_descriptor< value_node, 2 * N >;

        struct internal_children
        {
            using size_type = node_size_type;

            internal_children(internal_node* node)
                : node_(node)
                , size_()
            {
                auto keys = node_->get_keys();
                if (!keys.empty())
                {
                    size_ = keys.size() + 1;
                }
            }

            internal_children(const internal_children& other) = default;

            size_type size() const { return size_; }
            
            void set_size(size_type size) 
            { 
                assert(size <= capacity());
                size_ = size; 
            }

            size_type capacity() const { return 2 * N; }

            node** data() { return reinterpret_cast<node**>(node_->children); }
            node** data() const { return reinterpret_cast<node**>(node_->children); }

        private:
            internal_node* node_;
            size_type size_;
        };

    public:
        struct iterator
        {
            friend class set< Key, Compare, Allocator >;

            iterator(value_node* n, node_size_type nindex, node_size_type kindex)
                : node_(n)
                , nindex_(nindex)
                , kindex_(kindex)
            {}

            bool operator == (const iterator& rhs) const { return node_ == rhs.node_ && kindex_ == rhs.kindex_; }
            bool operator != (const iterator& rhs) const { return !(*this == rhs); }

            const Key& operator*() const { return node_->get_keys()[kindex_]; }
            const Key* operator -> () const { return node_->get_keys()[kindex_]; }

            iterator& operator ++ ()
            {
                if (++kindex_ == node_->get_keys().size())
                {
                    kindex_ = 0;
                    std::tie(node_, nindex_) = node_->get_right(nindex_, true);
                }

                return *this;
            }

            iterator operator++(int)
            {
                iterator it = *this;
                ++*this;
                return it;
            }

        private:
            value_node* node_;
            node_size_type nindex_;
            node_size_type kindex_;
        };

        set()
            : root_()
            , size_()
            , depth_()
        {
        #if defined(_DEBUG)
            // Make sure the objects alias.
            value_node v;
            assert(&v == (node*)&v);
            internal_node n;
            assert(&n == (node*)&n);
        #endif
        }

        ~set()
        {
            if (root_)
            {
                free_node(root_, 1);
            }
        }

        iterator find(const Key& key)
        {
            return root_ ? find(root_, key) : end();
        }

        template < typename KeyT > std::pair< iterator, bool > insert(KeyT&& key)
        {
            if (!root_)
            {
                root_ = allocate_node< value_node >();
                depth_ = 1;
            }

            auto [n, nindex] = find_value_node(root_, key);
            if (!n->full())
            {
                return insert(n, nindex, std::forward< KeyT >(key));
            }
            else
            {
                if (n->get_parent())
                {
                    std::tie(n, nindex) = rebalance_insert(depth_, n, nindex, key);
                    return insert(n, nindex, std::forward< KeyT >(key));
                }
                else
                {
                    std::tie(n, nindex) = rebalance_insert(depth_, n, key);
                    return insert(n, nindex, std::forward< KeyT >(key));
                }
            }
        }

        void erase(const Key& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase(it);
            }
        }

        void erase(iterator it)
        {
            assert(it != end());

            auto nkeys = it.node_->get_keys();
            if (it.node_->get_parent())
            {
                if (nkeys.size() > nkeys.capacity() / 2)
                {
                    nkeys.erase(allocator_, nkeys.begin() + it.kindex_);
                    --size_;
                }
                else
                {
                    auto key = it.node_->get_keys()[it.kindex_];
                    auto [n, nindex] = rebalance_erase(depth_, it.node_, it.nindex_);
                    auto nkeys = n->get_keys();
                    nkeys.erase(allocator_, find_key_index(nkeys, key));
                    --size_;
                }
            }
            else
            {
                nkeys.erase(allocator_, nkeys.begin() + it.kindex_);
                --size_;
            }
        }

        size_type size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() 
        {
            return iterator(empty() ? nullptr : begin_node<value_node>(root_, depth_), 0, 0); 
        }

        iterator end() { return iterator(nullptr, 0, 0); }

    private:
        std::tuple< value_node*, node_size_type > find_value_node(node* n, const Key& key)
        {
            size_type depth = depth_;
            node_size_type nindex = 0;
            while (--depth)
            {
                auto in = reinterpret_cast<internal_node*>(n);

                auto nkeys = in->get_keys();
                auto kindex = find_key_index(nkeys, key);
                if (kindex != nkeys.end())
                {
                    nindex = kindex - nkeys.begin() + !compare_(key, *kindex);
                }
                else
                {
                    nindex = nkeys.size();
                }

                n = in->get_children< node* >()[nindex];
            }

            return { reinterpret_cast<value_node*>(n), nindex };
        }

        iterator find(node* n, const Key& key)
        {
            auto [vn, vnindex] = find_value_node(n, key);
            assert(vn);

            auto nkeys = vn->get_keys();
            auto index = find_key_index(nkeys, key);
            if (index < nkeys.end() && key == *index)
            {
                return iterator(vn, vnindex, index - nkeys.begin());
            }
            else
            {
                return end();
            }
        }

        template < typename KeyT > std::pair< iterator, bool > insert(value_node* n, node_size_type nindex, KeyT&& key)
        {
            assert(!n->full());

            auto nkeys = n->get_keys();
            auto index = find_key_index(nkeys, key);
            if (index < nkeys.end() && *index == key)
            {
                return { iterator(n, nindex, index - nkeys.begin()), false };
            }
            else
            {
                nkeys.emplace(allocator_, index, std::forward< KeyT >(key));
                ++size_;

                return { iterator(n, nindex, index - nkeys.begin()), true };
            }
        }

        // TODO: const
        template < typename Descriptor > const Key* find_key_index(const fixed_vector< Key, Descriptor >& keys, const Key& key)
        {
            // TODO: better search
            auto index = keys.begin();
            for (; index != keys.end(); ++index)
            {
                if (!compare_(*index, key))
                {
                    break;
                }
            }

            return index;
        }

        template < typename Node > static auto find_node_index(const fixed_vector< Node*, internal_children >& nodes, const node* n)
        {
            for (decltype(nodes.size()) i = 0; i < nodes.size(); ++i)
            {
                if (nodes[i] == n)
                {
                    return i;
                }
            }

            return nodes.size();
        }

        template< typename Node > void remove_node(size_type depth, internal_node* parent, const Node* n, node_size_type nindex, node_size_type kindex)
        {
            auto pchildren = parent->get_children< node* >();
            pchildren.erase(allocator_, pchildren.begin() + nindex);
            
            auto pkeys = parent->get_keys();
            pkeys.erase(allocator_, pkeys.begin() + kindex);

            if (pkeys.empty())
            {
                auto root = root_;
                fixed_vector< node*, internal_children > pchildren(parent, 1); // override the size to 1
                root_ = pchildren[0];
                --depth_;
                assert(depth_ >= 1);
                set_parent(depth_ == 1, root_, nullptr);

                deallocate_node(reinterpret_cast< internal_node* >(root));
            }
        }

        template < typename Node > auto get_node_allocator()
        {
            return std::allocator_traits< Allocator >::rebind_alloc< Node >(allocator_);
        }

        template < typename Node > Node* allocate_node()
        {
            static_assert(!std::is_same_v<Node, node>);
            
            auto allocator = get_node_allocator< Node >();
            auto ptr = std::allocator_traits< decltype(allocator) >::allocate(allocator, 1);
            std::allocator_traits< decltype(allocator) >::construct(allocator, ptr);
            return ptr;
        }

        template < typename Node > void deallocate_node(Node* n)
        {
            static_assert(!std::is_same_v<Node, node>);
            n->cleanup(allocator_);

            auto allocator = get_node_allocator< Node >();
            std::allocator_traits< decltype(allocator) >::destroy(allocator, n);
            std::allocator_traits< decltype(allocator) >::deallocate(allocator, n, 1);
        }

        template < typename Node > const Key split_key(/*const*/ Node* n)
        {
            assert(n->full());
            return *(n->get_keys().begin() + N);
        }

        std::tuple< internal_node*, Key > split_node(size_type depth, internal_node* lnode)
        {
            assert(lnode->full());
            auto rnode = allocate_node< internal_node >();

            auto lchildren = lnode->get_children< node* >();
            auto rchildren = rnode->get_children< node* >();
            
            assert(depth_ >= depth + 1);
            rchildren.insert(allocator_, rchildren.begin(), lchildren.begin() + N, lchildren.end());
            std::for_each(lchildren.begin() + N, lchildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, rnode); });

            auto lkeys = lnode->get_keys();
            auto rkeys = rnode->get_keys();

            // TODO: std::make_move_iterator
            auto begin = lkeys.begin() + N;
            rkeys.insert(allocator_, rkeys.end(), std::make_move_iterator(begin), std::make_move_iterator(lkeys.end()));

            Key splitkey = std::move(*(begin - 1));

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(allocator_, begin - 1, lkeys.end());
            assert(lkeys.size() == N - 1);
            assert(rkeys.size() == N - 1);
       
            return { rnode, splitkey };
        }

        std::tuple< value_node*, Key > split_node(size_type, value_node* lnode)
        {
            assert(lnode->full());
            auto rnode = allocate_node< value_node >();

            auto lkeys = lnode->get_keys();
            auto rkeys = rnode->get_keys();

            // TODO: std::make_move_iterator
            auto begin = lkeys.begin() + N;
            rkeys.insert(allocator_, rkeys.end(), std::make_move_iterator(begin), std::make_move_iterator(lkeys.end()));

            // Keep splitkey.
            lkeys.erase(allocator_, begin, lkeys.end());
            assert(lkeys.size() == N);
            assert(rkeys.size() == N);

            return { rnode, *rkeys.begin() };
        }

        void free_node(node* n, size_type depth)
        {
            if (depth != depth_)
            {
                auto in = reinterpret_cast<internal_node*>(n);
                auto nchildren = in->get_children< node* >();

                for (auto child: nchildren)
                {
                    free_node(child, depth + 1);
                }

                deallocate_node(in);
            }
            else
            {
                deallocate_node(reinterpret_cast< value_node* >(n));
            }
        }

        template < typename Node > static Node* begin_node(node* n, size_type depth)
        {
            assert(n);
            while (--depth)
            {
                n = reinterpret_cast<internal_node*>(n)->get_children< node* >()[0];
            }

            return reinterpret_cast<Node*>(n);
        }

        template < typename Node > static Node* end_node(node* n, size_type depth)
        {
            assert(n);
            while (--depth)
            {
                auto children = reinterpret_cast<internal_node*>(n)->get_children< node* >();
                n = children[children.size() - 1];
            }

            return reinterpret_cast<Node*>(n);
        }

        bool share_keys(size_t, value_node* target, node_size_type tindex, value_node* source, node_size_type sindex)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source->get_keys();
            auto tkeys = target->get_keys();
            auto pkeys = target->get_parent()->get_keys();

            if (skeys.size() > N && tkeys.size() < 2*N)
            {
                if (tindex > sindex)
                {
                    // Right-most key from the left node
                    auto key = skeys.end() - 1;
                    tkeys.emplace(allocator_, tkeys.begin(), *key);
                    skeys.erase(allocator_, key);

                    assert(tindex > 0);
                    pkeys[tindex - 1] = *tkeys.begin();
                }
                else
                {
                    // Left-most key from the right node
                    auto key = skeys.begin();
                    tkeys.emplace(allocator_, tkeys.end(), *key);
                    skeys.erase(allocator_, key);

                    pkeys[tindex] = *skeys.begin();
                }

                return true;
            }

            return false;
        }

        bool share_keys(size_type depth, internal_node* target, node_size_type tindex, internal_node* source, node_size_type sindex)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source->get_keys(); 
            auto tkeys = target->get_keys();
            auto pkeys = target->get_parent()->get_keys();

            assert(depth_ >= depth + 1);

            if (skeys.size() > N - 1 && tkeys.size() < 2*N - 1)
            {
                auto schildren = source->get_children< node* >();
                auto tchildren = target->get_children< node* >();

                if (tindex > sindex)
                {
                    tchildren.emplace(allocator_, tchildren.begin(), schildren[skeys.size()]);
                    set_parent(depth_ == depth + 1, schildren[skeys.size()], target);

                    tkeys.emplace(allocator_, tkeys.begin(), pkeys[sindex]);    

                    pkeys[sindex] = *(skeys.end() - 1);
                    skeys.erase(allocator_, skeys.end() - 1);
                }
                else
                {
                    tkeys.emplace(allocator_, tkeys.end(), pkeys[tindex]);
                    
                    auto ch = schildren[0];
                    set_parent(depth_ == depth + 1, ch, target);

                    schildren.erase(allocator_, schildren.begin());
                    tchildren.emplace(allocator_, tchildren.end(), ch);
                    
                    pkeys[tindex] = *skeys.begin();
                    skeys.erase(allocator_, skeys.begin());
                }

                return true;
            }

            return false;
        }

        bool merge_keys(size_type depth, value_node* target, node_size_type tindex, value_node* source, node_size_type sindex)
        {
            if (!target)
            {
                return false;
            }

            auto tkeys = target->get_keys();
            if (tkeys.size() == N)
            {
                auto skeys = source->get_keys();
                tkeys.insert(allocator_, tindex < sindex ? tkeys.end() : tkeys.begin(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                return true;
            }

            return false;
        }

        bool merge_keys(size_type depth, internal_node* target, node_size_type tindex, internal_node* source, node_size_type sindex)
        {
            if (!target)
            {
                return false;
            }

            auto skeys = source->get_keys();
            auto tkeys = target->get_keys();
            auto pkeys = target->get_parent()->get_keys();

            if (tkeys.size() == N - 1)
            {
                assert(depth_ >= depth + 1);

                auto schildren = source->get_children< node* >();
                auto tchildren = target->get_children< node* >();
                
                if (tindex < sindex)
                {
                    tchildren.insert(allocator_, tchildren.end(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator_, tkeys.end(), pkeys[sindex - 1]);
                    tkeys.insert(allocator_, tkeys.end(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                }
                else
                {
                    tchildren.insert(allocator_, tchildren.begin(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { set_parent(depth_ == depth + 1, n, target); });

                    tkeys.emplace(allocator_, tkeys.begin(), pkeys[sindex]);
                    tkeys.insert(allocator_, tkeys.begin(), std::make_move_iterator(skeys.begin()), std::make_move_iterator(skeys.end()));
                }

                return true;
            }

            return false;
        }

        template < typename Node > std::tuple< Node*, node_size_type > rebalance_insert(size_type depth, Node* n, const Key& key)
        {
            assert(n->full());
            if(n->get_parent())
            {
                auto nindex = find_node_index(n->get_parent()->get_children< node* >(), n);
                return rebalance_insert(depth, n, nindex, key);
            }
            else
            {
                auto root = allocate_node< internal_node >();
                auto [p, splitkey] = split_node(depth, n);

                auto children = root->get_children< node* >();
                children.emplace_back(allocator_, n);
                n->set_parent(root);
                children.emplace_back(allocator_, p);
                p->set_parent(root);

                root->get_keys().emplace_back(allocator_, splitkey);

                root_ = root;
                ++depth_;

                auto cmp = !compare_(key, splitkey);
                return { cmp ? p : n, cmp };
            }
        }

        template < typename Node > std::tuple< Node*, node_size_type > rebalance_insert(size_type depth, Node* n, node_size_type nindex, const Key& key)
        {
            assert(n->full());
            assert(n->get_parent());

            auto parent_rebalance = n->get_parent()->full();
            if(parent_rebalance)
            {
                assert(depth > 1);
                depth_check< true > dc(depth_, depth);
                rebalance_insert(depth - 1, n->get_parent(), split_key(n->get_parent()));
                assert(!n->get_parent()->full());
            }
                    
            auto [p, splitkey] = split_node(depth, n);
                    
            auto pchildren = n->get_parent()->get_children< node* >();
            if (parent_rebalance)
            {
                nindex = find_node_index(pchildren, n);
            }

            pchildren.emplace(allocator_, pchildren.begin() + nindex + 1, p);
            p->set_parent(n->get_parent());

            auto pkeys = n->get_parent()->get_keys();
            pkeys.emplace(allocator_, pkeys.begin() + nindex, splitkey);

            auto cmp = !compare_(key, splitkey);
            return { cmp ? p : n, nindex + cmp };
        }

        template < typename Node > void rebalance_erase(size_type depth, Node* n)
        {
            if (n->get_parent())
            {
                auto nindex = find_node_index(n->get_parent()->get_children< node* >(), n);
                rebalance_erase(depth, n, nindex);
            }
        }

        template < typename Node > std::tuple< Node*, node_size_type > rebalance_erase(size_type depth, Node* n, node_size_type nindex)
        {
            assert(n->get_parent());
            assert(n->get_keys().size() <= n->get_keys().capacity() / 2);

            {
                auto [left, lindex] = n->get_left(nindex);
                auto [right, rindex] = n->get_right(nindex);

                if (left && share_keys(depth, n, nindex, left, lindex) ||
                    right && share_keys(depth, n, nindex, right, rindex))
                {
                    assert(n->get_keys().size() > n->get_keys().capacity() / 2);
                    return { n, nindex };
                }
            }

            auto pkeys = n->get_parent()->get_keys();
            if (pkeys.size() <= pkeys.capacity() / 2)
            {
                depth_check< false > dc(depth_, depth);
                rebalance_erase(depth - 1, n->get_parent());
                nindex = find_node_index(n->get_parent()->get_children< node* >(), n);
            }                
                
            {
                auto [left, lindex] = n->get_left(nindex);
                auto [right, rindex] = n->get_right(nindex);

                if (merge_keys(depth, left, lindex, n, nindex))
                {
                    remove_node(depth, n->get_parent(), n, nindex, nindex - 1);
                    deallocate_node(n);
                    return { left, nindex - 1 };
                }
                else if (merge_keys(depth, right, rindex, n, nindex))
                {
                    remove_node(depth, n->get_parent(), n, nindex, nindex);
                    deallocate_node(n);
                    return { right, nindex + 1 };
                }

                assert(false);
                std::abort();
                //return { n, nindex };
            }
        }

        template< typename Node > static std::tuple< Node*, node_size_type > get_right(internal_node* n, node_size_type index, bool recursive)
        {
            size_type depth = 1;
            while (n)
            {
                auto children = n->get_children< Node* >();
                if (index + 1 < children.size())
                {
                    if (depth == 1)
                    {
                        return { children[index + 1], index + 1 };
                    }
                    else
                    {
                        return { begin_node< Node >(children[index + 1], depth), 0 };
                    }
                }
                else if (recursive)
                {
                    if (n->get_parent())
                    {
                        index = find_node_index(n->get_parent()->get_children< internal_node* >(), n);
                        ++depth;
                    }

                    n = n->get_parent();
                }
                else
                {
                    break;
                }
            }

            return { nullptr, 0 };
        }

        template< typename Node > static std::tuple< Node*, node_size_type > get_left(internal_node* n, node_size_type index, bool recursive)
        {
            size_type depth = 1;
            while (n)
            {
                auto children = n->get_children< Node* >();
                if (index > 0)
                {
                    if (depth == 1)
                    {
                        return { children[index - 1], index - 1 };
                    }
                    else
                    {
                        return { end_node< Node >(children[index - 1], depth), 0 };
                    }
                }
                else if (recursive)
                {
                    if (n->get_parent())
                    {
                        index = find_node_index(n->get_parent()->get_children< internal_node* >(), n);
                        ++depth;
                    }

                    n = n->get_parent();
                }
                else
                {
                    break;
                }
            }

            return { nullptr, 0 };
        }

        static void set_parent(bool valuenode, node* n, internal_node* parent)
        {
        #if defined(_DEBUG)
            bool isvaluenode = dynamic_cast<value_node*>(n);
            assert(valuenode == isvaluenode);
        #endif

            if (valuenode)
            {
                reinterpret_cast<value_node*>(n)->set_parent(parent);
            }
            else
            {
                reinterpret_cast<internal_node*>(n)->set_parent(parent);
            }
        }

        node* root_;

        size_type size_;
        size_type depth_;
        
        Allocator allocator_;
        Compare compare_;
    };
}