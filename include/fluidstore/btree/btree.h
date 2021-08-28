#pragma once

#include <set>

namespace btree
{
    template < typename T, typename Descriptor > struct fixed_vector
    {
    public:
        fixed_vector(Descriptor desc) 
            : desc_(desc)
        {
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

        T* begin() { return desc_.data(); }
        T* end() { return desc_.data() + desc_.size(); }

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
        
        void insert(T* index, T* from, T* to)
        {
            assert(begin() <= index && index <= end());
            assert(to - from + index - begin() <= capacity());
            
            if (index < end())
            {
                std::memmove(index + (to - from), index, sizeof(T) * (end() - index));
            }
            
            std::memmove(index, from, sizeof(T) * (to - from));
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

    template < typename T, typename Descriptor > struct node_vector
    {
        node_vector(Descriptor desc)
            : size_(desc.size())
            , desc_(desc)
        {}

        node_vector(Descriptor desc, size_t size)
            : size_(size)
            , desc_(desc)
        {}

        void insert(int index, T* node)
        {
            assert(size() + 1 <= capacity());
            auto data = desc_.data();

            for (size_t i = index; i < size_; ++i)
            {
                data[i + 1] = data[i];
            }
            //node->index = index;
            node->set_parent(desc_.get_parent());
            data[index] = node;
            ++size_;
        }

        void insert(int index, T** from, T** to)
        {
            assert(0 <= index && index <= size());
            assert(to - from + index <= capacity());
            auto data = desc_.data();

            if (index < size())
            {
                for (int i = size() - 1;  i >= index; --i)
                {
                    data[i + (to - from)] = data[i];
                }
            }
        
            for (size_t i = 0; i < to - from; ++i)
            {
                data[index + i] = *(from + i);
                data[index + i]->set_parent(desc_.get_parent());
            }

            size_ += to - from;
        }

        void erase(int index)
        {
            assert(index < size());
            assert(size() >= 2);

            auto data = desc_.data();
            for (size_t i = index; i < size() - 1; ++i)
            {
                data[i] = data[i + 1];
            }
            --size_;
        }

        void push_back(T* node)
        {
            assert(size() + 1 < capacity());
            
            node->set_parent(desc_.get_parent());

            auto data = desc_.data();
            data[size_++] = node;
        }

        T* operator[](size_t index)
        {
            assert(index < size());
            return reinterpret_cast< T** >(desc_.data())[index];
        }

        size_t size() const { return size_; }
        size_t capacity() const { return desc_.capacity(); }
        
        T** begin() { return desc_.data(); }
        T** end() { return desc_.data() + size(); }

    private:
        Descriptor desc_;
        size_t size_;
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

        struct metadata
        {
            internal_node* parent;
            uint8_t size;
        };

        // In the end, this class will have no data and just provide casting capability.
        struct node
        {
        protected:
            node()
                : parent()
            {}

        public:
            template< typename Node > static Node* get_left(internal_node* parent, size_t index)
            {
                if (parent && index > 0)
                {
                    node_vector< node, node_vector_descriptor > pchildren(parent);
                    return reinterpret_cast< Node* >(pchildren[index - 1]);
                }

                return nullptr;
            }

            template< typename Node > static Node* get_right(internal_node* parent, size_t index)
            {
                if (parent)
                {
                    fixed_vector< Key, internal_node_descriptor > pkeys(parent);
                    if (index + 1 <= pkeys.size())
                    {
                        node_vector< node, node_vector_descriptor > pchildren(parent);
                        return reinterpret_cast< Node* >(pchildren[index + 1]);
                    }
                }

                return nullptr;
            }

            internal_node* get_parent() { return parent; }
            void set_parent(internal_node* n) { parent = n; }

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

            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            std::array< node*, 2 * N > children;
            uint8_t size;
        };

        struct value_node : node
        {
            value_node()
                : size()
            {}

            value_node* get_left(size_t index) { return node::get_left< value_node >(parent, index); }
            value_node* get_right(size_t index) { return node::get_right< value_node >(parent, index); }

            uint8_t keys[2 * N * sizeof(Key)];
            // uint8_t values[2 * N * sizeof(Value)];
            uint8_t size;
        };

        template < typename Node, size_t Capacity > struct node_descriptor
        {
            node_descriptor(Node* node)
                : node_(node)
            {}

            size_t size() const { return node_->size; }
            
            void set_size(size_t size)
            {
                assert(size <= capacity());
                node_->size = size;
            }

            size_t capacity() const { return Capacity; }

            Key* data() { return reinterpret_cast<Key*>(node_->keys); }
            Node* get_node() { return node_; }

        private:
            Node* node_;
        };

        using internal_node_descriptor = node_descriptor< internal_node, 2 * N - 1 >;
        using value_node_descriptor = node_descriptor< value_node, 2 * N >;

        struct node_vector_descriptor
        {
            node_vector_descriptor(internal_node* node)
                : desc_(node)
            {}

            node_vector_descriptor(const node_vector_descriptor& other) = default;

            size_t size() { return desc_.size() ? desc_.size() + 1 : 0; }
            size_t capacity() const { return 2 * N; }

            node** data()
            {
                auto data = desc_.get_node()->children.data();
                return reinterpret_cast<node**>(data);
            }

            internal_node* get_parent() { return desc_.get_node(); }

        private:
            internal_node_descriptor desc_;
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

            bool operator == (const iterator& rhs) const
            {
                return node_ == rhs.node_ && i_ == rhs.i_;
            }

            bool operator != (const iterator& rhs) const
            {
                return !(*this == rhs);
            }

            const Key& operator*() const
            {
                fixed_vector< Key, value_node_descriptor > keys(node_);
                return keys[i_];
            }

            const Key* operator -> () const
            {
                fixed_vector< Key, value_node_descriptor > keys(node_);
                return keys_[i_];
            }

            iterator& operator ++ ()
            {
                fixed_vector< Key, value_node_descriptor > keys(node_);
                if (++i_ == keys.size())
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
                free_node(root_, depth_);
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
                ++depth_;
            }

            auto [n, nindex] = find_value_node(root_, key);
            fixed_vector< Key, value_node_descriptor > nkeys(n);
            if (nkeys.size() < nkeys.capacity())
            {
                return insert(n, nindex, std::forward< KeyT >(key));
            }
            else
            {
                auto [s, skey] = split_node(n);
                rebalance_insert(n, nindex, s, skey);

                node_vector< value_node, node_vector_descriptor > pchildren(n->get_parent());
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

            fixed_vector< Key, value_node_descriptor > nkeys(it.node_);
            nkeys.erase(nkeys.begin() + it.i_);

            rebalance_erase(it.node_, it.nindex_);
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

                fixed_vector< Key, internal_node_descriptor > nkeys(in);
                node_vector< node, node_vector_descriptor > nchildren(in);

                auto kindex = find_key_index(nkeys, key);
                if (kindex != nkeys.end())
                {
                    nindex = kindex - nkeys.begin() + !compare_(key, *kindex);
                }
                else
                {
                    nindex = nkeys.size();
                }

                n = nchildren[nindex];
            }

            return { reinterpret_cast<value_node*>(n), nindex };
        }

        iterator find(node* n, const Key& key)
        {
            auto [vn, vnindex] = find_value_node(n, key);

            fixed_vector< Key, value_node_descriptor > nkeys(vn);
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
            fixed_vector< Key, value_node_descriptor > nkeys(n);
            assert(nkeys.size() < nkeys.capacity());

            auto index = find_key_index(nkeys, key);

            if (index < nkeys.end() && *index == key)
            {
                return { iterator(reinterpret_cast<value_node*>(n), nindex, index - nkeys.begin()), false };
            }
            else
            {
                nkeys.insert(index, std::forward< KeyT >(key));
                ++size_;

                return { iterator(reinterpret_cast<value_node*>(n), nindex, index - nkeys.begin()), true };
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

        template < typename Node > size_t find_node_index(/*const */node_vector< Node, node_vector_descriptor >& nodes, const node* n)
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

        void remove_node(internal_node* parent, node* n, size_t nindex, int key_index)
        {
            fixed_vector< Key, internal_node_descriptor > pkeys(parent);
            
            node_vector< node, node_vector_descriptor > pchildren(parent);
            pchildren.erase(nindex);
                        
            pkeys.erase(pkeys.begin() + key_index);

            if (pkeys.size() < N - 1)
            {
                rebalance_erase(parent);
            }
        }

        template < typename Node > Node* allocate_node()
        {
            return new Node;
        }

        void deallocate_node(internal_node* n)
        {
            delete n;
        }

        void deallocate_node(value_node* n)
        {
            delete n;
        }

        template < typename Node > Node* node_cast(node* n)
        {
            return reinterpret_cast<Node*>(n);
        }

        std::tuple< node*, Key > split_node(internal_node* lnode)
        {
            auto rnode = allocate_node< internal_node >();

            // Split children
            node_vector< node, node_vector_descriptor > lchildren(lnode);
            node_vector< node, node_vector_descriptor > rchildren(rnode);
            
            rchildren.insert(0, lchildren.begin() + N, lchildren.end());

            fixed_vector< Key, internal_node_descriptor > lkeys(lnode);
            fixed_vector< Key, internal_node_descriptor > rkeys(rnode);

            auto begin = lkeys.begin() + N;
            rkeys.insert(rkeys.end(), begin, lkeys.end());

            Key splitkey = *(begin - 1);

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(begin - 1, lkeys.end());
            assert(lkeys.size() == N - 1);
            assert(rkeys.size() == N - 1);
       
            return { rnode, splitkey };
        }

        std::tuple< node*, Key > split_node(value_node* lnode)
        {
            auto rnode = allocate_node< value_node >();

            fixed_vector< Key, value_node_descriptor > lkeys(lnode);
            fixed_vector< Key, value_node_descriptor > rkeys(rnode);

            auto begin = lkeys.begin() + N;
            rkeys.insert(rkeys.end(), begin, lkeys.end());

            // Keep splitkey.
            lkeys.erase(begin, lkeys.end());
            assert(lkeys.size() == N);
            assert(rkeys.size() == N);

            return { rnode, *rkeys.begin() };
        }

        void rebalance_insert(node* l, size_t lindex, node* r, Key key)
        {
            auto p = l->get_parent();
            if (p)
            {
                fixed_vector< Key, internal_node_descriptor > pkeys(p);
                if (pkeys.size() < pkeys.capacity())
                {
                    node_vector< node, node_vector_descriptor > pchildren(p);
                    pchildren.insert(lindex + 1, r);
                    pkeys.insert(pkeys.begin() + lindex, key);
                }
                else
                {
                    auto [q, pkey] = split_node(p);
                    
                    size_t pindex = -1;
                    if (p->get_parent())
                    {
                        node_vector< node, node_vector_descriptor > pchildren(p->get_parent());
                        pindex = find_node_index(pchildren, p);
                    }

                    rebalance_insert(p, pindex, q, pkey);

                    // TODO: This just retries the call after making space.
                    node_vector< node, node_vector_descriptor > pchildren(l->get_parent());
                    rebalance_insert(l, find_node_index(pchildren, l), r, key);
                }
            }
            else
            {
                auto root = allocate_node< internal_node >();

                node_vector< node, node_vector_descriptor > children(root);
                children.push_back(l);
                children.push_back(r);

                fixed_vector< Key, internal_node_descriptor > rkeys(root);
                rkeys.push_back(key);

                root_ = root;
                ++depth_;
            }
        }

        void rebalance_erase(value_node* n, size_t nindex)
        {
            fixed_vector< Key, value_node_descriptor > nkeys(n);
            if (nkeys.size() < N)
            {
                if (n->get_parent())
                {
                    auto left = n->get_left(nindex);
                    auto right = n->get_right(nindex);

                    if (borrow_keys(n, nindex, true, left, nindex - 1) ||
                        borrow_keys(n, nindex, false, right, nindex + 1))
                    {
                        return;
                    }

                    if (merge_keys(left, true, n, nindex))
                    {
                        remove_node(n->get_parent(), n, nindex, nindex - 1);
                        deallocate_node(n);
                        return;
                    }

                    if(merge_keys(right, false, n, nindex))
                    {
                        remove_node(n->get_parent(), n, nindex, nindex);
                        deallocate_node(n);
                        return;
                    }
                }
            }
        }

        void rebalance_erase(internal_node* n)
        {
            fixed_vector< Key, internal_node_descriptor > nkeys(n);
            if (nkeys.size() < N - 1)
            {
                if (n->get_parent())
                {
                    node_vector< node, node_vector_descriptor > pchildren(n->get_parent());
                    size_t nindex = find_node_index(pchildren, n);

                    auto left = n->get_left(nindex);
                    auto right = n->get_right(nindex);

                    if (borrow_keys(n, nindex, true, left, nindex - 1) ||
                        borrow_keys(n, nindex, false, right, nindex + 1))
                    {
                        return;
                    }

                    if (merge_keys(left, true, n, nindex))
                    {
                        remove_node(n->get_parent(), n, nindex, nindex - 1);
                        deallocate_node(n);
                        return;
                    }

                    if (merge_keys(right, false, n, nindex))
                    {
                        remove_node(n->get_parent(), n, nindex, nindex);
                        deallocate_node(n);
                        return;
                    }

                    assert(false);
                }
                else if(nkeys.empty())
                {
                    node_vector< node, node_vector_descriptor > nchildren(n, 1); // override the size to 1
                    root_ = nchildren[0];
                    root_->set_parent(nullptr);
                    --depth_;

                    deallocate_node(n);
                }
            }
        }

        void free_node(node* n, size_t depth)
        {
            if (--depth > 0)
            {
                auto in = reinterpret_cast<internal_node*>(n);

                fixed_vector< Key, internal_node_descriptor > nkeys(in);
                node_vector< node, node_vector_descriptor > nchildren(in);

                for (auto child: nchildren)
                {
                    free_node(child, depth);
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
                node_vector< node, node_vector_descriptor > children(reinterpret_cast<internal_node*>(n));
                n = children[0];
            }

            return reinterpret_cast<value_node*>(n);
        }

        bool borrow_keys(value_node* target, size_t tindex, bool left, value_node* source, size_t sindex)
        {
            if (!source)
            {
                return false;
            }

            fixed_vector< Key, value_node_descriptor > skeys(source);
            fixed_vector< Key, value_node_descriptor > tkeys(target);
            fixed_vector< Key, internal_node_descriptor > pkeys(target->get_parent());

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

        bool borrow_keys(internal_node* target, size_t tindex, bool left, internal_node* source, size_t sindex)
        {
            if (!source)
            {
                return false;
            }

            fixed_vector< Key, internal_node_descriptor > skeys(source);
            fixed_vector< Key, internal_node_descriptor > tkeys(target);
            fixed_vector< Key, internal_node_descriptor > pkeys(target->get_parent());

            if (skeys.size() > N - 1)
            {
                node_vector< node, node_vector_descriptor > schildren(source);
                node_vector< node, node_vector_descriptor > tchildren(target);

                if (left)
                {
                    tchildren.insert(0, schildren[0]);

                    tkeys.insert(tkeys.begin(), pkeys[sindex]);    

                    pkeys[sindex] = *(skeys.end() - 1);
                    skeys.erase(skeys.end() - 1);
                }
                else
                {
                    tkeys.insert(tkeys.end(), pkeys[tindex]);
                    
                    auto ch = schildren[0];
                    schildren.erase(0);
                    tchildren.insert(tchildren.size(), ch);

                    pkeys[tindex] = *skeys.begin();
                    skeys.erase(skeys.begin());
                }

                return true;
            }

            return false;
        }

        bool merge_keys(value_node* target, bool left, value_node* source, size_t sindex)
        {
            if (!target)
            {
                return false;
            }

            fixed_vector< Key, value_node_descriptor > skeys(source);
            fixed_vector< Key, value_node_descriptor > tkeys(target);

            if (tkeys.size() == N)
            {
                tkeys.insert(left ? tkeys.end() : tkeys.begin(), skeys.begin(), skeys.end());
                return true;
            }

            return false;
        }

        bool merge_keys(internal_node* target, bool left, internal_node* source, size_t sindex)
        {
            if (!target)
            {
                return false;
            }

            fixed_vector< Key, internal_node_descriptor > skeys(source);
            fixed_vector< Key, internal_node_descriptor > tkeys(target);
            fixed_vector< Key, internal_node_descriptor > pkeys(target->get_parent());

            if (tkeys.size() == N - 1)
            {
                node_vector< node, node_vector_descriptor > schildren(source);
                node_vector< node, node_vector_descriptor > tchildren(target);
                if (left)
                {
                    tchildren.insert(tchildren.size(), schildren.begin(), schildren.end());

                    tkeys.insert(tkeys.end(), pkeys[sindex - 1]);
                    tkeys.insert(tkeys.end(), skeys.begin(), skeys.end());
                }
                else
                {
                    tchildren.insert(0, schildren.begin(), schildren.end());

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