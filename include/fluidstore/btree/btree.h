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

        void erase_to_end(T* begin)
        {
            assert(begin <= end());
            desc_.set_size(size() - (end() - begin));
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
        
        struct node 
        {
            node()
                : meta()
            {}

                // 64 bytes
            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            uint8_t meta;
                
            bool is_internal() const { return meta & 1; }
        };

        struct internal_node: node
        {
            internal_node()
            {
                meta = 1;
            }

            std::array< node*, 2 * N > children;
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

        bool find(const Key& key)
        {
            return root_ ? find(root_, key) : false;
        }

        template < typename KeyT > void insert(KeyT&& key)
        {
            if (!root_)
            {
                root_ = allocate_node< node >();
            }

            fixed_vector< Key, node_descriptor > keys(root_);
            if (keys.size() < keys.capacity())
            {
                insert(root_, std::forward< KeyT >(key));
            }
            else
            {
                // TODO: return split key in a nicer way...
                Key splitkey = *(keys.end() - N);
                auto n = split_node(root_);
             
                auto root = allocate_node< internal_node >();
                root->children[0] = root_;
                root->children[1] = n;

                fixed_vector< Key, node_descriptor > rkeys(root);
                fixed_vector< Key, node_descriptor > nkeys(n);
                rkeys.push_back(splitkey);

                root_ = root;

                insert(root->children[compare_(splitkey, key)], std::forward< KeyT >(key));
            }
        }

        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

    private:

        bool find(node* n, const Key& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);

            auto index = find_index(nkeys, key);
            
            auto i = index - nkeys.begin();
            if (i < nkeys.size() && key == nkeys[i])
            {
                return true;
            }

            // TODO: recursion
            if (n->is_internal())
            {
                auto in = reinterpret_cast<internal_node*>(n);
                return find(in->children[i], key);
            }
            else
            {
                return false;
            }
        }

        template < typename KeyT > void insert(node* n, KeyT&& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);
            assert(nkeys.size() < nkeys.capacity());

            auto index = find_index(nkeys, key);

            if (!n->is_internal())
            {
                nkeys.insert(index, std::forward< KeyT >(key));
                ++size_;
            }
            else
            {
                auto p = index - nkeys.begin();
                internal_node* in = reinterpret_cast< internal_node* >(n);
                auto cnode = in->children[p];

                fixed_vector< Key, node_descriptor > ckeys(cnode);
                if (ckeys.size() == ckeys.capacity())
                {
                    // TODO: return splitkey in a better way.
                    Key splitkey = *(ckeys.end() - N);
                    node* dnode = split_node(cnode);

                    // TODO: better move
                    for (size_t i = nkeys.size(); i > p; --i)
                    {
                        in->children[i + 1] = in->children[i];
                    }
                    in->children[p + 1] = dnode;

                    nkeys.insert(index, splitkey);

                    insert(in->children[p + compare_(splitkey, key)], std::forward< KeyT >(key));
                }
                else
                {
                    insert(cnode, std::forward< KeyT >(key));
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
        
        node* split_node(node* lnode)
        {
            node* rnode = 0;
            if (lnode->is_internal())
            {
                rnode = allocate_node< internal_node >();
            }
            else
            {
                rnode = allocate_node< node >();
            }

            fixed_vector< Key, node_descriptor > lkeys(lnode);
            fixed_vector< Key, node_descriptor > rkeys(rnode);

            // TODO: better move support, integers should be memcpy-ed as a range, strings moved one by one, etc.
            auto begin = lkeys.begin() + N;
            for (auto it = begin; it != lkeys.end(); ++it)
            {
                rkeys.push_back(*it);
            }

            // Remove splitkey, too (begin - 1). Each node should end up with N-1 keys.
            lkeys.erase_to_end(begin - 1);

            assert(lkeys.size() == N - 1);
            assert(rkeys.size() == N - 1);

            if (lnode->is_internal())
            {
                auto linode = reinterpret_cast<internal_node*>(lnode);
                auto rinode = reinterpret_cast<internal_node*>(rnode);

                // split children
                for (size_t i = 0; i < N; ++i)
                {
                    rinode->children[i] = linode->children[i + N];
                }
            }

            // TODO: return split key, too.
            return rnode;
        }

        void free_node(node* n)
        {
            if (n->is_internal())
            {
                fixed_vector< Key, node_descriptor > nkeys(n);

                auto in = reinterpret_cast<internal_node*>(n);
                for (size_t i = 0; i < nkeys.size() + 1; ++i)
                {
                    free_node(in->children[i]);
                }
            }

            deallocate_node(n);
        }

        node* root_;
        size_t size_;
        Allocator allocator_;
        Compare compare_;
    };
}