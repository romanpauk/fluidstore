namespace btree
{
    template < typename T, typename Descriptor > struct fixed_vector
    {
    public:
        fixed_vector(Descriptor desc) :
            desc_(desc)
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
            
            // TODO: this assumes std layout type
            std::memmove(it + 1, it, desc_.size() - index);

            *it = std::forward< Ty >(value);
            desc_.set_size(desc_.size() + 1);
        }
        
        T& operator[](size_t index)
        {
            return *(begin() + index);
        }

    private:
        Descriptor desc_;
    };

    template < typename Key, typename Compare = std::less< Key >, typename Allocator = std::allocator< Key > > class set
    {
        static const size_t KeyCount = 7; // 64 / sizeof(Key);

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

            uint8_t keys[KeyCount * sizeof(Key)];
            uint8_t meta;
                // node type, keys size

            bool is_internal() const { return meta & 1; }
        };

        struct internal_node: node
        {
            internal_node()
            {
                meta = 1;
            }

            std::array< node*, KeyCount + 1 > children;
        };

        struct node_descriptor
        {
            node_descriptor() = default;
          
            node_descriptor(node* node)
                : node_(node)
            {}

            size_t size()
            {
                // extract size from metadata
                return node_->meta >> 1;
            }

            void set_size(size_t size)
            {
                // store size to metadata
                node_->meta = (size << 1) | (node_->meta & 1);
            }

            size_t capacity() { return KeyCount; }

            Key* data() { return reinterpret_cast< Key* >(node_->keys); }

        private:
            node* node_;
        };

    public:
        set()
            : root_()
            , size_()
        {}

        bool find(const Key& key)
        {
            if (root_)
            {
                return find(root_, key);
            }
            else
            {
                return false;
            }
        }

        template < typename KeyT > void insert(KeyT&& key)
        {
            if (!root_)
            {
                root_ = allocate_node< node >();
                assert(!root_->is_internal());
            }

            fixed_vector< Key, node_descriptor > keys(root_);
            if (keys.size() < keys.capacity())
            {
                insert(root_, std::forward< KeyT >(key));
            }
            else
            {
                auto n = split_node(root_);
             
                auto root = allocate_node< internal_node >();
                root->children[0] = root_;
                root->children[1] = n;

                fixed_vector< Key, node_descriptor > rkeys(root);
                fixed_vector< Key, node_descriptor > nkeys(n);
                rkeys.push_back(*nkeys.begin());

                root_ = root;

                insert(root->children[compare_(*rkeys.begin(), key)], std::forward< KeyT >(key));
            }
        }

        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }

    private:

        bool find(node* n, const Key& key)
        {
            fixed_vector< Key, node_descriptor > nkeys(n);

            auto index = nkeys.begin();
            for (; index != nkeys.end(); ++index)
            {
                if (!compare_(*index, key))
                {
                    break;
                }
            }

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

            auto index = nkeys.begin();
            for (; index != nkeys.end(); ++index)
            {
                if (!compare_(*index, key))
                {
                    break;
                }
            }

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
                    node* dnode = split_node(cnode);

                    // insert dnode into children
                    
                    for (size_t i = nkeys.size(); i > p; --i)
                    {
                        in->children[i + 1] = in->children[i];
                    }
                    in->children[p + 1] = dnode;

                    fixed_vector< Key, node_descriptor > dkeys(dnode);
                    nkeys.insert(index, *dkeys.begin());

                    insert(in->children[p + compare_(*dkeys.begin(), key)], std::forward< KeyT >(key));
                }
                else
                {
                    insert(cnode, std::forward< KeyT >(key));
                }
            }
        }

        template < typename Node > Node* allocate_node()
        {
            return new Node;
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

            // move KeyCount/2 keys from n to newnode
            fixed_vector< Key, node_descriptor > lkeys(lnode);
            fixed_vector< Key, node_descriptor > rkeys(rnode);

            // TODO: better move support, integers should be memcpy-ed as a range, strings moved one by one, etc.
            auto begin = lkeys.begin() + KeyCount / 2;
            for (auto it = begin; it != lkeys.end(); ++it)
            {
                rkeys.push_back(*it);
            }

            lkeys.erase_to_end(begin);

            if (lnode->is_internal())
            {
                auto linode = reinterpret_cast<internal_node*>(lnode);
                auto rinode = reinterpret_cast<internal_node*>(rnode);

                // split children
                for (size_t i = 0; i < rkeys.size() + 1; ++i)
                {
                    rinode->children[i] = linode->children[i + KeyCount / 2];
                }
            }

            return rnode;
        }

        node* root_;
        size_t size_;
        Allocator allocator_;
        Compare compare_;
    };
}