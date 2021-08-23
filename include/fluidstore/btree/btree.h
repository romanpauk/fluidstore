#pragma once

#include <set>

namespace btree
{
    template < typename T, typename Descriptor > struct fixed_vector
    {
    public:
        fixed_vector(Descriptor desc) 
            : desc_(desc)
        {}

        void push_back(T value)
        {
            assert(size() < capacity());

            auto index = size();
            new(desc_.data() + index) T(value);
            desc_.set_size(index + 1);
        }

        void erase(T* index)
        {
            assert(begin() <= index && index < end());
            // TODO:
            std::memmove(index, index + 1, end() - index);
            desc_.set_size(size() - 1);
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
        }

        auto size() { return desc_.size(); }
        auto capacity() { return desc_.capacity(); }

        T* begin() { return desc_.data(); }
        T* end() { return desc_.data() + desc_.size(); }

        template < typename Ty > void insert(T* it, Ty&& value)
        {
            assert(size() < capacity());

            auto index = it - desc_.data();
            assert(index <= size());

            // TODO: this assumes std layout type
            std::memmove(it + 1, it, desc_.size() - index);

            *it = std::forward< Ty >(value);
            desc_.set_size(desc_.size() + 1);
        }
        
        void insert(T* index, T* from, T* to)
        {
            assert(begin() <= index && index <= end());
            assert(from < to);
            assert(to - from <= capacity() - size());

            // TODO
            std::memmove(index + (to - from), from, to - from);
            std::memmove(index, from, to - from);
            desc_.set_size(size() + to - from);
        }

        T& operator[](size_t index)
        {
            assert(index < size());
            return *(begin() + index);
        }

    private:
        Descriptor desc_;
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
            node()
                : meta()
                , parent()
                , index()
            {}

            // The idea is that those are 64byte aligned, are they?
            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            uint8_t meta;
            
            internal_node* parent;
            uint8_t index;

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

        struct internal_node: node
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

            
        private:
            std::array< node*, 2 * N > children;
        };

        struct value_node : node
        {
            value_node()
            {}

            // uint8_t values[(2 * N - 1) * sizeof(Value)];
            // uint8_t meta;
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

            size_t capacity() { return 2 * N - 1; }

            Key* data() { return reinterpret_cast< Key* >(node_->keys); }

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
                    node_ = reinterpret_cast< value_node* >(node_->get_right());
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
        {}

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

            fixed_vector< Key, node_descriptor > keys(root_);
            if (keys.size() < keys.capacity())
            {
                return insert(root_, std::forward< KeyT >(key));
            }
            else
            {
                auto [n, splitkey] = split_node(root_);
             
                auto root = allocate_node< internal_node >();
                root->add_node(root_, 0); 
                root->add_node(n, 1); 
                
                fixed_vector< Key, node_descriptor > rkeys(root);
                rkeys.push_back(splitkey);

                root_ = root;

                return insert(root->get_node(compare_(splitkey, key)), std::forward< KeyT >(key));
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

            if (nkeys.size() < N - 1)
            {
                if (node->parent)
                {
                    auto left = node->get_left();
                    auto right = node->get_right();

                    // For value nodes:
                    //      If left has more than N - 1, take left key
                    //      If right has more than N - 1, take right key
                    if (rotate_key(node, true, left) ||
                        rotate_key(node, false, right))
                    {
                        return;
                    }

                    //      If left has exactly N - 1, merge with left
                    //      If right has exactly N - 1, merge with right
                    if (merge_keys(left, true, node) ||
                        merge_keys(right, false, node))
                    {
                        //  Rebalance parent as we will also remove the key in parent

                    }
                }
            }
        }

        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() { return iterator(empty() ? nullptr : begin_node(), 0); }
        iterator end() { return iterator(nullptr, 0); }

    private:
        iterator find(node* n, const Key& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);

            auto index = find_index(nkeys, key);
            
            auto i = index - nkeys.begin();
            
            // TODO: recursion
            if (n->is_internal())
            {
                auto in = reinterpret_cast<internal_node*>(n);
                return find(in->get_node(i), key);
            }
            else
            {
                if (i < nkeys.size() && key == nkeys[i])
                {
                    return iterator(reinterpret_cast< value_node* >(n), i);
                }
                else
                {
                    return end();
                }
            }
        }

        template < typename KeyT > std::pair< iterator, bool > insert(node* n, KeyT&& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);
            assert(nkeys.size() < nkeys.capacity());

            auto index = find_index(nkeys, key);

            if (!n->is_internal())
            {
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
            else
            {
                auto p = index - nkeys.begin();
                internal_node* in = reinterpret_cast< internal_node* >(n);
                auto cnode = in->get_node(p);

                fixed_vector< Key, node_descriptor > ckeys(cnode);
                if (ckeys.size() == ckeys.capacity())
                {
                    auto [dnode, splitkey] = split_node(cnode);

                    // TODO: better move
                    for (size_t i = nkeys.size(); i > p; --i)
                    {
                        in->add_node(in->get_node(i), i + 1);
                    }
                    in->add_node(dnode, p + 1);

                    nkeys.insert(index, splitkey);

                    return insert(in->get_node(p + compare_(splitkey, key)), std::forward< KeyT >(key));
                }
                else
                {
                    return insert(cnode, std::forward< KeyT >(key));
                }
            }
        }

        Key* find_index(fixed_vector< Key, node_descriptor >& keys, const Key& key)
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
                delete n;
            }
        }

        template < typename Node > Node* node_cast(node* n) 
        { 
            return reinterpret_cast< Node* >(n); 
        }
        
        std::tuple< node*, Key > split_node(node* lnode)
        {
            node* rnode = 0;
            if (lnode->is_internal())
            {
                rnode = allocate_node< internal_node >();
            }
            else
            {
                rnode = allocate_node< value_node >();
            }

            fixed_vector< Key, node_descriptor > lkeys(lnode);
            fixed_vector< Key, node_descriptor > rkeys(rnode);

            // TODO: better move support, integers should be memcpy-ed as a range, strings moved one by one, etc.
            auto begin = lkeys.begin() + N;
            for (auto it = begin; it != lkeys.end(); ++it)
            {
                rkeys.push_back(*it);
            }

            Key splitkey = *(begin - 1);
            if (lnode->is_internal())
            {
                // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys as split key will be propagated to parent node.
                lkeys.erase(begin - 1, lkeys.end());
                assert(lkeys.size() == N - 1);
            }
            else
            {
                // Keep splitkey.
                lkeys.erase(begin, lkeys.end());
                assert(lkeys.size() == N);
            }

            assert(rkeys.size() == N - 1);

            if (lnode->is_internal())
            {
                auto linode = reinterpret_cast<internal_node*>(lnode);
                auto rinode = reinterpret_cast<internal_node*>(rnode);

                // split children
                for (size_t i = 0; i < N; ++i)
                {
                    rinode->add_node(linode->get_node(i + N), i);
                }
            }

            return { rnode, splitkey };
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

        bool rotate_key(node* target, bool left, node* source)
        {
            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);
            fixed_vector< Key, node_descriptor > pkeys(target->parent);

            if (skeys.size() > N - 1)
            {
                if (left)
                {
                    // Right-most key from the left node
                    auto key = skeys.end() - 1;
                    tkeys.insert(tkeys.begin(), *key);
                    skeys.erase(key);

                    // This is wrong...
                    pkeys[target->index] = *(skeys.end() - 1);
                }
                else
                {
                    // Left-most key from the right node
                    auto key = skeys.begin();
                    tkeys.insert(tkeys.end(), *key);
                    skeys.erase(key);

                    pkeys[target->index - 1] = *skeys.begin();
                }

                return true;
            }

            return false;
        }

        bool merge_keys(node* target, bool left, node* source)
        {
            fixed_vector< Key, node_descriptor > skeys(source);
            fixed_vector< Key, node_descriptor > tkeys(target);

            if (tkeys.size() == N - 1)
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

        node* root_;
        size_t size_;
        Allocator allocator_;
        Compare compare_;
    };
}