#pragma once

#include <set>

namespace btree
{
    template < typename T, typename Descriptor > struct fixed_vector
    {
    public:
        //fixed_vector(const fixed_vector< T, Descriptor >&) = delete;
        //fixed_vector(fixed_vector< T, Descriptor >&&) = delete;

        fixed_vector(Descriptor desc) 
            : desc_(desc)
        {
            checkvec();
        }

        fixed_vector(Descriptor desc, size_t size)
            : desc_(desc)
        {
            desc_.set_size(size);
            checkvec();
        }

        void push_back(T value)
        {
            assert(size() < capacity());

            auto index = size();
            new(desc_.data() + index) T(value);
            desc_.set_size(index + 1);

            checkvec();
        }

        void erase(T* index)
        {
            assert(begin() <= index && index < end());
            
            // TODO:
            std::memmove(index, index + 1, sizeof(T) * (end() - index));
            desc_.set_size(size() - 1);

            checkvec();
        }

        void erase(T* from, T* to)
        {
            assert(begin() <= from && from <= to);
            assert(from <= to && to <= end());

            if (to == end())
            {
                desc_.set_size(size() - (to - from));
            }
            else
            {
                std::abort(); // TODO
            }

            checkvec();
        }

        auto size() { return desc_.size(); }
        auto capacity() { return desc_.capacity(); }
        auto empty() { return desc_.size() == 0; }

        // TODO
        T* begin() { return reinterpret_cast< T* >(desc_.data()); }
        T* end() { return reinterpret_cast< T* >(desc_.data()) + desc_.size(); }

        template < typename Ty > void insert(T* it, Ty&& value)
        {
            assert(size() < capacity());
            assert(it >= begin());
            assert(it <= end());

            auto index = it - desc_.data();
            
            if (!empty())
            {
                // TODO
                std::memmove(it + 1, it, sizeof(T) * (desc_.size() - index));
            }

            *it = std::forward< Ty >(value);
            desc_.set_size(desc_.size() + 1);

            checkvec();
        }
        
        void insert(T* it, T* from, T* to)
        {
            assert(begin() <= it && it <= end());
            assert((uintptr_t)(to - from + it - begin()) <= capacity());
            
            if (it < end())
            {
                std::memmove(it + (to - from), it, sizeof(T) * (end() - it));
            }
            
            std::memmove(it, from, sizeof(T) * (to - from));
            desc_.set_size(size() + to - from);

            checkvec();
        }

        T& operator[](size_t index)
        {
            assert(index < size());
            return *(begin() + index);
        }

    private:
        void checkvec()
        {
        #if defined(_DEBUG)
            assert(end() - begin() == size());
            assert(size() <= capacity());

            // vec_.assign(begin(), end());
        
            if (vec_.size() > 1)
            {
                for (size_t i = 0; i < size() - 1; ++i)
                {
                    if (vec_[i] >= vec_[i + 1])
                    {
                        assert(false);
                    }
                }
            }
        #endif
        }

        Descriptor desc_;

    #if defined(_DEBUG)
        std::vector< T > vec_;
    #endif
    };
    template < typename Key, typename Compare = std::less< Key >, typename Allocator = std::allocator< Key > > class set
    {
        static const size_t N = 4;

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

        struct internal_node;

        // In the end, this class will have no data and just provide casting capability.
        struct node
        {
        protected:
            node()
                : parent()
            {}

        #if defined(_DEBUG)
            virtual ~node() {}
        #endif

        public:
            template< typename Node > static Node* get_left(internal_node* parent, size_t index)
            {
                if (parent && index > 0)
                {
                    fixed_vector< Node*, internal_children > pchildren(parent);
                    return pchildren[index - 1];
                }

                return nullptr;
            }

            template< typename Node > static Node* get_right(internal_node* parent, size_t index)
            {
                if (parent)
                {
                    if (index + 1 <= parent->get_keys().size())
                    {
                        fixed_vector< Node*, internal_children > pchildren(parent);
                        return pchildren[index + 1];
                    }
                }

                return nullptr;
            }

            void set_parent(bool valuenode, internal_node* n)
            {
            #if defined(_DEBUG)
                bool isvaluenode = dynamic_cast<value_node*>(this);
                assert(valuenode == isvaluenode);
            #endif

                /*
                if (valuenode)
                {
                    reinterpret_cast<value_node*>(this)->set_parent(n);
                }
                else
                {
                    reinterpret_cast<internal_node*>(this)->set_parent(n);
                }
                */
                parent = n; 
            }

        private:
            template < typename Node, size_t Capacity > friend struct node_descriptor;
            friend struct iterator;

        protected:
            internal_node* parent;
        };

        struct internal_node : node
        {
            internal_node()
                : size()
            {}

            internal_node* get_left(size_t index) { return node::get_left< internal_node >(parent, index); }
            internal_node* get_right(size_t index) { return node::get_right< internal_node >(parent, index); }
            auto get_keys() { return fixed_vector< Key, internal_keys >(this); }
            internal_node* get_parent() { return parent; }
            void set_parent(internal_node* n) { parent = n; }
            // template < typename Node > auto get_children() { return fixed_vector< Node, internal_children >(this); }

            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            uint8_t children[2 * N * sizeof(node*)];

            size_t size; // TODO: this needs to be smaller, and elsewhere.
        };

        struct value_node : node
        {
            value_node()
                : size()
            {}

            value_node* get_left(size_t index) { return node::get_left< value_node >(parent, index); }
            value_node* get_right(size_t index) { return node::get_right< value_node >(parent, index); }
            auto get_keys() { return fixed_vector< Key, value_keys >(this); }
            internal_node* get_parent() { return parent; }
            void set_parent(internal_node* n) { parent = n; }

            uint8_t keys[2 * N * sizeof(Key)];
            // uint8_t values[2 * N * sizeof(Value)];
            size_t size; // TODO: this needs to be smaller, and elsewhere.
        };

        template < typename Node, size_t Capacity > struct keys_descriptor
        {
            keys_descriptor(Node* node)
                : node_(node)
            {}

            size_t size() const { return node_->size; }
            size_t capacity() const { return Capacity; }

            void set_size(size_t size)
            {
                assert(size <= capacity());
                node_->size = size;
            }
           
            Key* data() { return reinterpret_cast<Key*>(node_->keys); }
            Node* get_node() { return node_; }

        private:
            Node* node_;
        };

        using internal_keys = keys_descriptor< internal_node, 2 * N - 1 >;
        using value_keys = keys_descriptor< value_node, 2 * N >;

        struct internal_children
        {
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

            size_t size() { return size_; }
            void set_size(size_t size) { size_ = size; }
            size_t capacity() const { return 2 * N; }

            node** data()
            {
                auto data = node_->children;
                return reinterpret_cast<node**>(data);
            }

        private:
            internal_node* node_;
            size_t size_;
        };

        struct context
        {
            size_t depth;
        };

    public:
        struct iterator
        {
            friend class set< Key, Compare, Allocator >;

            iterator(value_node* n, size_t nindex, size_t i)
                : node_(n)
                , nindex_(nindex)
                , i_(i)
            {}

            bool operator == (const iterator& rhs) const { return node_ == rhs.node_ && i_ == rhs.i_; }
            bool operator != (const iterator& rhs) const { return !(*this == rhs); }

            const Key& operator*() const { return node_->get_keys()[i_]; }
            const Key* operator -> () const { return node_->get_keys()[_i]; }

            iterator& operator ++ ()
            {
                if (++i_ == node_->get_keys().size())
                {
                    i_ = 0;
                    node_ = node_->get_right(nindex_);
                    ++nindex_;
                }

                return *this;
            }

            iterator operator++(int)
            {
                iterator it = *this;
                ++* this;
                return it;
            }

        private:
            value_node* node_;
            size_t nindex_;
            size_t i_;
        };

        set()
            : root_()
            , size_()
            , depth_()
        {
            // Make sure the objects alias.
            value_node v;
            assert(&v == (node*)&v);
            internal_node n;
            assert(&n == (node*)&n);
        }

        ~set()
        {
            if (root_)
            {
                free_node(1, root_);
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
            auto nkeys = n->get_keys();
            if (nkeys.size() < nkeys.capacity())
            {
                return insert(n, nindex, std::forward< KeyT >(key));
            }
            else
            {
                auto [s, skey] = split_node(n);
                rebalance_insert(depth_, n, nindex, s, skey);

                fixed_vector< value_node*, internal_children > pchildren(n->get_parent());
                nindex = find_node_index(pchildren, n);

                return insert(pchildren[nindex + compare_(skey, key)], nindex, std::forward< KeyT >(key));
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
            nkeys.erase(nkeys.begin() + it.i_);

            rebalance_erase(depth_, it.node_, it.nindex_);
        }

        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() { return iterator(empty() ? nullptr : begin_node(), 0, 0); }
        iterator end() { return iterator(nullptr, 0, 0); }

    private:
        std::tuple< value_node*, size_t > find_value_node(node* n, const Key& key)
        {
            size_t depth = depth_;
            size_t nindex = 0;
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

                fixed_vector< node*, internal_children > nchildren(in);
                n = nchildren[nindex];
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

        template < typename KeyT > std::pair< iterator, bool > insert(value_node* n, size_t nindex, KeyT&& key)
        {
            auto nkeys = n->get_keys();
            assert(nkeys.size() < nkeys.capacity());

            auto index = find_key_index(nkeys, key);
            if (index < nkeys.end() && *index == key)
            {
                return { iterator(n, nindex, index - nkeys.begin()), false };
            }
            else
            {
                nkeys.insert(index, std::forward< KeyT >(key));
                ++size_;

                return { iterator(n, nindex, index - nkeys.begin()), true };
            }
        }

        // TODO: const
        template < typename Descriptor > Key* find_key_index(/*const */fixed_vector< Key, Descriptor >& keys, const Key& key)
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

        template < typename Node > size_t find_node_index(/*const */fixed_vector< Node*, internal_children >& nodes, const node* n)
        {
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                if (nodes[i] == n)
                {
                    return i;
                }
            }

            return nodes.size();
        }

        void remove_node(size_t depth, internal_node* parent, node* n, size_t nindex, size_t key_index)
        {
            fixed_vector< node*, internal_children > pchildren(parent);
            pchildren.erase(pchildren.begin() + nindex);
            
            auto pkeys = parent->get_keys();
            pkeys.erase(pkeys.begin() + key_index);

            if (pkeys.size() < N - 1)
            {
                assert(depth > 1);
                rebalance_erase(depth - 1, parent);
            }
        }

        template < typename Node > Node* allocate_node()
        {
            return new Node;
        }

        template < typename Node > void deallocate_node(Node* n)
        {
            delete n;
        }

        std::tuple< internal_node*, Key > split_node(size_t depth, internal_node* lnode)
        {
            auto rnode = allocate_node< internal_node >();

            // Split children
            fixed_vector< node*, internal_children > lchildren(lnode);
            fixed_vector< node*, internal_children > rchildren(rnode);
            
            assert(depth_ >= depth + 1);
            rchildren.insert(rchildren.begin(), lchildren.begin() + N, lchildren.end());
            std::for_each(lchildren.begin() + N, lchildren.end(), [&](auto& n) { n->set_parent(depth_ == depth + 1, rnode); });

            auto lkeys = lnode->get_keys();
            auto rkeys = rnode->get_keys();

            auto begin = lkeys.begin() + N;
            rkeys.insert(rkeys.end(), begin, lkeys.end());

            Key splitkey = *(begin - 1);

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(begin - 1, lkeys.end());
            assert(lkeys.size() == N - 1);
            assert(rkeys.size() == N - 1);
       
            return { rnode, splitkey };
        }

        std::tuple< value_node*, Key > split_node(value_node* lnode)
        {
            auto rnode = allocate_node< value_node >();

            auto lkeys = lnode->get_keys();
            auto rkeys = rnode->get_keys();

            auto begin = lkeys.begin() + N;
            rkeys.insert(rkeys.end(), begin, lkeys.end());

            // Keep splitkey.
            lkeys.erase(begin, lkeys.end());
            assert(lkeys.size() == N);
            assert(rkeys.size() == N);

            return { rnode, *rkeys.begin() };
        }

        template < typename Node > void rebalance_insert(size_t depth, Node* l, size_t lindex, Node* r, Key key)
        {
            auto p = l->get_parent();
            if (p)
            {
                auto pkeys = p->get_keys();
                if (pkeys.size() < pkeys.capacity())
                {
                    fixed_vector< node*, internal_children > pchildren(p);
                    pchildren.insert(pchildren.begin() + lindex + 1, r);
                    r->set_parent(p);

                    pkeys.insert(pkeys.begin() + lindex, key);
                }
                else
                {
                    assert(depth > 1);
                    auto [q, pkey] = split_node(depth - 1, p);
                    
                    size_t pindex = -1;
                    if (p->get_parent())
                    {
                        fixed_vector< node*, internal_children > pchildren(p->get_parent());
                        pindex = find_node_index(pchildren, p);
                    }

                    assert(depth > 1);
                    rebalance_insert(depth - 1, p, pindex, q, pkey);

                    // TODO: This just retries the call after making space.
                    fixed_vector< node*, internal_children > pchildren(l->get_parent());
                    rebalance_insert(depth, l, find_node_index(pchildren, l), r, key);
                }
            }
            else
            {
                assert(depth == 1);

                auto root = allocate_node< internal_node >();
                fixed_vector< node*, internal_children > children(root);

                children.push_back(l);
                l->set_parent(root);

                children.push_back(r);
                r->set_parent(root);

                root->get_keys().push_back(key);

                root_ = root;
                ++depth_;
            }
        }

        void rebalance_erase(size_t depth, value_node* n, size_t nindex)
        {
            if (n->get_keys().size() < N)
            {
                if (n->get_parent())
                {
                    auto left = n->get_left(nindex);
                    auto right = n->get_right(nindex);

                    if (share_keys(depth, n, nindex, true, left, nindex - 1) ||
                        share_keys(depth, n, nindex, false, right, nindex + 1))
                    {
                        return;
                    }

                    if (merge_keys(depth, left, true, n, nindex))
                    {
                        remove_node(depth, n->get_parent(), n, nindex, nindex - 1);
                        deallocate_node(n);
                        return;
                    }

                    if(merge_keys(depth, right, false, n, nindex))
                    {
                        remove_node(depth, n->get_parent(), n, nindex, nindex);
                        deallocate_node(n);
                        return;
                    }
                }
            }
        }

        void rebalance_erase(size_t depth, internal_node* n)
        {
            auto nkeys = n->get_keys();
            if (nkeys.size() < N - 1)
            {
                if (n->get_parent())
                {
                    fixed_vector< node*, internal_children > pchildren(n->get_parent());
                    size_t nindex = find_node_index(pchildren, n);

                    auto left = n->get_left(nindex);
                    auto right = n->get_right(nindex);

                    if (share_keys(depth, n, nindex, true, left, nindex - 1) ||
                        share_keys(depth, n, nindex, false, right, nindex + 1))
                    {
                        return;
                    }

                    if (merge_keys(depth, left, true, n, nindex))
                    {
                        remove_node(depth, n->get_parent(), n, nindex, nindex - 1);
                        deallocate_node(n);
                        return;
                    }

                    if (merge_keys(depth, right, false, n, nindex))
                    {
                        remove_node(depth, n->get_parent(), n, nindex, nindex);
                        deallocate_node(n);
                        return;
                    }

                    assert(false);
                }
                else if(nkeys.empty())
                {
                    fixed_vector< node*, internal_children > nchildren(n, 1); // override the size to 1
                    root_ = nchildren[0];
                    --depth_;
                    assert(depth_ >= 1);
                    root_->set_parent(depth_ == 1, nullptr);
                    deallocate_node(n);
                }
            }
        }

        void free_node(size_t depth, node* n)
        {
            if (depth != depth_)
            {
                auto in = reinterpret_cast<internal_node*>(n);

                auto nkeys = in->get_keys();
                fixed_vector< node*, internal_children > nchildren(in);

                for (auto child: nchildren)
                {
                    free_node(depth + 1, child);
                }

                deallocate_node(in);
            }
            else
            {
                deallocate_node(reinterpret_cast< value_node* >(n));
            }
        }

        value_node* begin_node() const
        {
            assert(root_);

            size_t depth = depth_;
            node* n = root_;
            while (--depth)
            {
                fixed_vector< node*, internal_children > children(reinterpret_cast<internal_node*>(n));
                n = children[0];
            }

            return reinterpret_cast<value_node*>(n);
        }

        bool share_keys(size_t, value_node* target, size_t tindex, bool left, value_node* source, size_t sindex)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source->get_keys();
            auto tkeys = target->get_keys();
            fixed_vector< Key, internal_keys > pkeys(target->get_parent());

            if (skeys.size() > N)
            {
                if (left)
                {
                    // Right-most key from the left node
                    auto key = skeys.end() - 1;
                    tkeys.insert(tkeys.begin(), *key);
                    skeys.erase(key);

                    assert(tindex > 0);
                    pkeys[tindex - 1] = *tkeys.begin();
                }
                else
                {
                    // Left-most key from the right node
                    auto key = skeys.begin();
                    tkeys.insert(tkeys.end(), *key);
                    skeys.erase(key);

                    pkeys[tindex] = *skeys.begin();
                }

                return true;
            }

            return false;
        }

        bool share_keys(size_t depth, internal_node* target, size_t tindex, bool left, internal_node* source, size_t sindex)
        {
            if (!source)
            {
                return false;
            }

            auto skeys = source->get_keys(); 
            auto tkeys = target->get_keys();
            fixed_vector< Key, internal_keys > pkeys(target->get_parent());

            assert(depth_ >= depth + 1);

            if (skeys.size() > N - 1)
            {
                fixed_vector< node*, internal_children > schildren(source);
                fixed_vector< node*, internal_children > tchildren(target);

                if (left)
                {
                    tchildren.insert(tchildren.begin(), schildren[0]);
                    schildren[0]->set_parent(depth_ == depth + 1, target);

                    tkeys.insert(tkeys.begin(), pkeys[sindex]);    

                    pkeys[sindex] = *(skeys.end() - 1);
                    skeys.erase(skeys.end() - 1);
                }
                else
                {
                    tkeys.insert(tkeys.end(), pkeys[tindex]);
                    
                    auto ch = schildren[0];
                    schildren.erase(schildren.begin());
                    tchildren.insert(tchildren.end(), ch);
                    ch->set_parent(depth_ == depth + 1, target);

                    pkeys[tindex] = *skeys.begin();
                    skeys.erase(skeys.begin());
                }

                return true;
            }

            return false;
        }

        bool merge_keys(size_t depth, value_node* target, bool left, value_node* source, size_t sindex)
        {
            if (!target)
            {
                return false;
            }

            auto tkeys = target->get_keys();
            if (tkeys.size() == N)
            {
                auto skeys = source->get_keys();
                tkeys.insert(left ? tkeys.end() : tkeys.begin(), skeys.begin(), skeys.end());
                return true;
            }

            return false;
        }

        bool merge_keys(size_t depth, internal_node* target, bool left, internal_node* source, size_t sindex)
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

                fixed_vector< node*, internal_children > schildren(source);
                fixed_vector< node*, internal_children > tchildren(target);
                if (left)
                {
                    tchildren.insert(tchildren.end(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { n->set_parent(depth_ == depth + 1, target); });

                    tkeys.insert(tkeys.end(), pkeys[sindex - 1]);
                    tkeys.insert(tkeys.end(), skeys.begin(), skeys.end());
                }
                else
                {
                    tchildren.insert(tchildren.begin(), schildren.begin(), schildren.end());
                    std::for_each(schildren.begin(), schildren.end(), [&](auto& n) { n->set_parent(depth_ == depth + 1, target); });

                    tkeys.insert(tkeys.begin(), pkeys[sindex]);
                    tkeys.insert(tkeys.begin(), skeys.begin(), skeys.end());
                }

                return true;
            }

            return false;
        }

        node* root_;

        size_t size_;
        size_t depth_;
        
        Allocator allocator_;
        Compare compare_;
    };
}