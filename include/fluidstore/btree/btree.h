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

    template < typename Key, typename Compare = std::less< Key >, typename Allocator = std::allocator< Key > > class set
    {
        static const size_t N = 4;

        // 32bit keys:
        //  11 * 4 = 44
        //  8 + 12 = 20
        //           64
        //    metadata in pointer
        // 
        // 64bit keys:
        //  6 * 8 = 48
        //  8 + 7 = 15
        //          63
        //    metadata in separate byte
        // 
        // One page - 64 * 64 (or ~60 + metadata, freelist etc)
        //   6 bits for addressing

        struct internal_node;

        struct node
        {
        protected:
            node()
                : meta()
                , parent()
                , index()
            {}

        public:
            internal_node* parent;
            uint8_t index;
            uint8_t meta;

            bool is_internal() const { return meta & 1; }

            node* get_left()
            {
                if (parent)
                {
                    if (index >= 1)
                    {
                        return parent->get_node(index - 1);
                    }
                }

                return nullptr;
            }

            node* get_right()
            {
                if (parent)
                {
                    fixed_vector< Key, node_descriptor > pkeys(parent);
                    if (index + 1 <= pkeys.size())
                    {
                        return parent->get_node(index + 1);
                    }
                }

                return nullptr;
            }
        };

        struct internal_node : node
        {
            internal_node()
            {
                meta = 1;
            }

            void add_node(node* n, int index)
            {
                n->parent = this;
                n->index = index;
                children[index] = n;
            }

            node* get_node(int index)
            {
                return children[index];
            }

            uint8_t keys[(2 * N - 1) * sizeof(Key)];

        private:
            std::array< node*, 2 * N > children;
        };

        struct value_node : node
        {
            value_node()
            {}

            uint8_t keys[2 * N * sizeof(Key)];
        };

        struct node_descriptor
        {
            node_descriptor() = default;

            node_descriptor(node* node)
                : node_(node)
            {}

            size_t size()
            {
                return node_->meta >> 1;
            }

            void set_size(size_t size)
            {
                assert(size <= capacity());
                node_->meta = (size << 1) | (node_->meta & 1);
            }

            size_t capacity() { return node_->is_internal() ? 2 * N - 1 : 2 * N; }

            Key* data()
            {
                auto keys = node_->is_internal() ? reinterpret_cast<internal_node*>(node_)->keys : reinterpret_cast<value_node*>(node_)->keys;
                return reinterpret_cast<Key*>(keys);
            }

        private:
            node* node_;
        };

    public:
        struct iterator
        {
            friend class set< Key, Compare, Allocator >;

            iterator(value_node* n, size_t i)
                : node_(n)
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
                fixed_vector< Key, node_descriptor > keys(node_);
                return keys[i_];
            }

            const Key* operator -> () const
            {
                fixed_vector< Key, node_descriptor > keys(node_);
                return keys_[i_];
            }

            iterator& operator ++ ()
            {
                fixed_vector< Key, node_descriptor > keys(node_);
                if (++i_ == keys.size())
                {
                    i_ = 0;
                    node_ = reinterpret_cast<value_node*>(node_->get_right());
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
            size_t i_;
        };

        set()
            : root_()
            , size_()
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
                free_node(root_);
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
            }

            auto n = find_value_node(root_, key);
            fixed_vector< Key, node_descriptor > nkeys(n);
            if (nkeys.size() < nkeys.capacity())
            {
                return insert(n, std::forward< KeyT >(key));
            }
            else
            {
                auto [s, skey] = split_node(n);
                rebalance_insert(n, s, skey);

                return insert(reinterpret_cast<value_node*>(n->parent->get_node(n->index + compare_(skey, key))), std::forward< KeyT >(key));
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
            auto node = it.node_;

            fixed_vector< Key, node_descriptor > nkeys(node);
            nkeys.erase(nkeys.begin() + it.i_);

            rebalance_erase(node);
        }

        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() { return iterator(empty() ? nullptr : begin_node(), 0); }
        iterator end() { return iterator(nullptr, 0); }

    private:
        value_node* find_value_node(node* n, const Key& key)
        {
            while (n->is_internal())
            {
                fixed_vector< Key, node_descriptor > nkeys(n);
                auto index = find(nkeys, key);
                auto in = reinterpret_cast<internal_node*>(n);

                if (index != nkeys.end())
                {
                    n = in->get_node(index - nkeys.begin() + !compare_(key, *index));
                }
                else
                {
                    n = in->get_node(nkeys.size());
                }               
            }

            return reinterpret_cast<value_node*>(n);
        }

        iterator find(node* n, const Key& key)
        {
            auto vn = find_value_node(n, key);

            fixed_vector< Key, node_descriptor > nkeys(vn);
            auto index = find(nkeys, key);
            if (index < nkeys.end() && key == *index)
            {
                return iterator(vn, index - nkeys.begin());
            }
            else
            {
                return end();
            }
        }

        template < typename KeyT > std::pair< iterator, bool > insert(value_node* n, KeyT&& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);
            assert(nkeys.size() < nkeys.capacity());

            auto index = find(nkeys, key);

            if (index < nkeys.end() && *index == key)
            {
                return { iterator(reinterpret_cast<value_node*>(n), index - nkeys.begin()), false };
            }
            else
            {
                nkeys.insert(index, std::forward< KeyT >(key));
                ++size_;

                return { iterator(reinterpret_cast<value_node*>(n), index - nkeys.begin()), true };
            }
        }

        // TODO: const
        Key* find(/*const */fixed_vector< Key, node_descriptor >& keys, const Key& key)
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

        void remove_node(internal_node* parent, node* n, int key_index)
        {
            fixed_vector< Key, node_descriptor > pkeys(parent);
            assert(n->index <= pkeys.size());

            for (size_t i = n->index; i < pkeys.size(); ++i)
            {
                parent->add_node(parent->get_node(i + 1), i);
            }
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

        void deallocate_node(node* n)
        {
            if (n->is_internal())
            {
                delete reinterpret_cast<internal_node*>(n);
            }
            else
            {
                delete reinterpret_cast<value_node*>(n);
            }
        }

        template < typename Node > Node* node_cast(node* n)
        {
            return reinterpret_cast<Node*>(n);
        }

        std::tuple< node*, Key > split_node(internal_node* lnode)
        {
            auto rnode = allocate_node< internal_node >();

            fixed_vector< Key, node_descriptor > lkeys(lnode);
            fixed_vector< Key, node_descriptor > rkeys(rnode);

            // TODO: better move support, integers should be memcpy-ed as a range, strings moved one by one, etc.
            auto begin = lkeys.begin() + N;
            for (auto it = begin; it != lkeys.end(); ++it)
            {
                rkeys.push_back(*it);
            }

            Key splitkey = *(begin - 1);

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
            lkeys.erase(begin - 1, lkeys.end());
            assert(lkeys.size() == N - 1);
            assert(rkeys.size() == N - 1);
       
            // Split children
            for (size_t i = 0; i < N; ++i)
            {
                rnode->add_node(lnode->get_node(i + N), i);
            }

            return { rnode, splitkey };
        }

        std::tuple< node*, Key > split_node(value_node* lnode)
        {
            auto rnode = allocate_node< value_node >();

            fixed_vector< Key, node_descriptor > lkeys(lnode);
            fixed_vector< Key, node_descriptor > rkeys(rnode);

            // TODO: better move support, integers should be memcpy-ed as a range, strings moved one by one, etc.
            auto begin = lkeys.begin() + N;
            for (auto it = begin; it != lkeys.end(); ++it)
            {
                rkeys.push_back(*it);
            }

            // Keep splitkey.
            lkeys.erase(begin, lkeys.end());
            assert(lkeys.size() == N);
            assert(rkeys.size() == N);

            return { rnode, *rkeys.begin() };
        }

        std::tuple< node*, Key > split_node(node* node)
        {
            if (node->is_internal())
            {
                return split_node(reinterpret_cast<internal_node*>(node));
            }
            else
            {
                return split_node(reinterpret_cast<value_node*>(node));
            }
        }

        void rebalance_insert(node* l, node* r, Key key)
        {
            auto p = l->parent;
            if (p)
            {
                fixed_vector< Key, node_descriptor > pkeys(p);
                if (pkeys.size() < pkeys.capacity())
                {
                    for (size_t i = pkeys.size(); i > l->index; --i)
                    {
                        p->add_node(p->get_node(i), i + 1);
                    }
                    p->add_node(r, l->index + 1);
                    pkeys.insert(pkeys.begin() + l->index, key);
                }
                else
                {
                    auto [q, pkey] = split_node(p);
                    rebalance_insert(p, q, pkey);

                    // TODO: This just retries the call after making space.
                    rebalance_insert(l, r, key);
                }
            }
            else
            {
                auto root = allocate_node< internal_node >();

                root->add_node(l, 0);
                root->add_node(r, 1);

                fixed_vector< Key, node_descriptor > rkeys(root);
                rkeys.push_back(key);

                root_ = root;
            }
        }

        void rebalance_erase(value_node* node)
        {
            fixed_vector< Key, node_descriptor > nkeys(node);
            if (nkeys.size() < N)
            {
                if (node->parent)
                {
                    auto left = reinterpret_cast< value_node* >(node->get_left());
                    auto right = reinterpret_cast< value_node* >(node->get_right());

                    if (borrow_keys(node, true, left) ||
                        borrow_keys(node, false, right))
                    {
                        return;
                    }

                    if (merge_keys(left, true, node))
                    {
                        remove_node(node->parent, node, node->index - 1);
                        return;
                    }

                    if(merge_keys(right, false, node))
                    {
                        remove_node(node->parent, node, node->index);
                        return;
                    }
                }
            }
        }

        void rebalance_erase(internal_node* node)
        {
            fixed_vector< Key, node_descriptor > nkeys(node);
            if (nkeys.size() < N - 1)
            {
                if (node->parent)
                {
                    auto left = reinterpret_cast<internal_node*>(node->get_left());
                    auto right = reinterpret_cast<internal_node*>(node->get_right());

                    if (borrow_keys(node, true, left) ||
                        borrow_keys(node, false, right))
                    {
                        return;
                    }

                    if (merge_keys(left, true, node))
                    {
                        remove_node(node->parent, node, node->index - 1);
                        return;
                    }

                    if (merge_keys(right, false, node))
                    {
                        remove_node(node->parent, node, node->index);
                        return;
                    }

                    assert(false);
                }
                else if(nkeys.empty())
                {
                    root_ = node->get_node(0);
                    root_->parent = 0;
                }
            }
        }

        void free_node(node* n)
        {
            if (n->is_internal())
            {
                fixed_vector< Key, node_descriptor > nkeys(n);

                auto in = reinterpret_cast<internal_node*>(n);
                for (size_t i = 0; i < nkeys.size() + 1; ++i)
                {
                    free_node(in->get_node(i));
                }
            }

            deallocate_node(n);
        }

        value_node* begin_node() const
        {
            assert(root_);

            node* n = root_;
            while (n->is_internal())
            {
                n = reinterpret_cast<internal_node*>(n)->get_node(0);
            }

            return reinterpret_cast<value_node*>(n);
        }

        bool borrow_keys(value_node* target, bool left, value_node* source)
        {
            if (!source)
            {
                return false;
            }

            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);
            fixed_vector< Key, node_descriptor > pkeys(target->parent);

            if (skeys.size() > N)
            {
                if (left)
                {
                    // Right-most key from the left node
                    auto key = skeys.end() - 1;
                    tkeys.insert(tkeys.begin(), *key);
                    skeys.erase(key);

                    assert(target->index > 0);
                    pkeys[target->index - 1] = *tkeys.begin();
                }
                else
                {
                    // Left-most key from the right node
                    auto key = skeys.begin();
                    tkeys.insert(tkeys.end(), *key);
                    skeys.erase(key);

                    pkeys[target->index] = *skeys.begin();
                }

                return true;
            }

            return false;
        }

        bool borrow_keys(internal_node* target, bool left, internal_node* source)
        {
            if (!source)
            {
                return false;
            }

            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);
            fixed_vector< Key, node_descriptor > pkeys(target->parent);

            if (skeys.size() > N - 1)
            {
                if (left)
                {
                    for (int i = 0; i < tkeys.size()+1; ++i)
                    {
                        target->add_node(target->get_node(i), i + 1);
                    }
                    target->add_node(source->get_node(skeys.size()), 0);

                    tkeys.insert(tkeys.begin(), pkeys[source->index]);    

                    pkeys[source->index] = *(skeys.end() - 1);
                    skeys.erase(skeys.end() - 1);
                }
                else
                {
                    tkeys.insert(tkeys.end(), pkeys[target->index]);
                    
                    target->add_node(source->get_node(0), tkeys.size());
                    for (int i = 0; i < skeys.size(); ++i)
                    {
                        source->add_node(source->get_node(i + 1), i);
                    }

                    pkeys[target->index] = *skeys.begin();
                    skeys.erase(skeys.begin());
                }

                return true;
            }

            return false;
        }

        bool merge_keys(value_node* target, bool left, value_node* source)
        {
            if (!target)
            {
                return false;
            }

            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);

            if (tkeys.size() == N)
            {
                if (left)
                {
                    tkeys.insert(tkeys.end(), skeys.begin(), skeys.end());
                }
                else
                {
                    tkeys.insert(tkeys.begin(), skeys.begin(), skeys.end());
                }

                return true;
            }

            return false;
        }

        bool merge_keys(internal_node* target, bool left, internal_node* source)
        {
            if (!target)
            {
                return false;
            }

            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);
            fixed_vector< Key, node_descriptor > pkeys(target->parent);

            if (tkeys.size() == N - 1)
            {
                if (left)
                {
                    for (int i = 0; i < skeys.size() + 1; ++i)
                    {
                        // +1 because of pkey[source->index]
                        target->add_node(source->get_node(i), tkeys.size() + i + 1);
                    }

                    tkeys.insert(tkeys.end(), pkeys[source->index - 1]);
                    tkeys.insert(tkeys.end(), skeys.begin(), skeys.end());
                }
                else
                {
                    for (int i = tkeys.size(); i >= 0; --i)
                    {
                        // +1 because of pkey[source->index]
                        target->add_node(target->get_node(i), i + skeys.size() + 1);
                    }

                    for (size_t i = 0; i < skeys.size() + 1; ++i)
                    {
                        target->add_node(source->get_node(i), i);
                    }

                    // TODO:
                    tkeys.insert(tkeys.begin(), pkeys[source->index]);
                    tkeys.insert(tkeys.begin(), skeys.begin(), skeys.end());
                }

                return true;
            }

            return false;
        }

        node* root_;
        size_t size_;
        Allocator allocator_;
        Compare compare_;
    };
}