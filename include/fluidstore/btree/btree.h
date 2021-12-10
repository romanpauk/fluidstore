#pragma once

#include <set>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <iterator>

#define BTREE_VALUE_NODE_LR
//#define BTREE_VALUE_NODE_APPEND
//#define BTREE_VALUE_NODE_HINT

#if defined(_DEBUG)
    //#define BTREE_CHECK_VECTOR_INVARIANTS
    #define BTREE_CHECK_TREE_INVARIANTS

    // TODO: try lambdas, but look at code generated for release.
    #define BTREE_CHECK(code) \
        do { \
            (code); \
            checktree(); \
        } while(0)

    #define BTREE_CHECK_RETURN(code) \
        do { \
            auto result = (code); \
            checktree(); \
            return result; \
        } while(0)
#else
    #define BTREE_CHECK(code) (code)
    #define BTREE_CHECK_RETURN(code) return (code);
#endif

namespace btree
{
    namespace detail 
    {
        // TODO: this could use at least one test...
        template < typename T, typename Descriptor > struct fixed_vector
        {
        public:
            using size_type = typename Descriptor::size_type;
            using value_type = T;
            using iterator = value_type*;

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

            template < typename Allocator, typename... Args > void emplace_back(Allocator& alloc, Args&&... args)
            {
                emplace(alloc, end(), std::forward< Args >(args)...);
            }

            template < typename Allocator > iterator erase(Allocator& alloc, iterator index)
            {
                assert(begin() <= index && index < end());
                
                bool last = index == end() - 1;
                auto dest = std::move(index + 1, end(), index);
                destroy(alloc, end()-1, end());
            
                desc_.set_size(size() - 1);

                checkvec();

                return last ? end() : index;
            }

            template < typename Allocator > void erase(Allocator& alloc , iterator from, iterator to)
            {
                assert(begin() <= from && from <= to);
                assert(from <= to && to <= end());

                if (to == end())
                {
                    destroy(alloc, from, to);
                    desc_.set_size(size() - static_cast< size_type >(to - from));
                }
                else
                {
                    std::abort(); // TODO
                }

                checkvec();
            }

            size_type size() const { return desc_.size(); }
            constexpr size_type capacity() const { return desc_.capacity(); }
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

            T& front() { return *begin(); }
            const T& front() const { return *begin(); }

            T& back() { return *(end() - 1); }
            const T& back() const { return *(end() - 1); }

            template < typename Allocator, typename... Args > void emplace(Allocator& alloc, iterator it, Args&&... args)
            {
                assert(size() < capacity());
                assert(it >= begin());
                assert(it <= end());

                if (it < end())
                {
                    move_backward(alloc, it, end(), end() + 1);

                    if constexpr (std::is_move_assignable_v< T >)
                    {
                        const_cast<T&>(*it) = T(std::forward< Args >(args)...);
                    }
                    else
                    {
                        std::allocator_traits< Allocator >::destroy(alloc, it);
                        std::allocator_traits< Allocator >::construct(alloc, it, std::forward< Args >(args)...);
                    }
                }
                else
                {
                    std::allocator_traits< Allocator >::construct(alloc, it, std::forward< Args >(args)...);
                }

                desc_.set_size(desc_.size() + 1);

                checkvec();
            }
        
            template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, U from, U to)
            {
                assert(begin() <= it && it <= end());
                assert((uintptr_t)(to - from + it - begin()) <= capacity());
            
                if (it < end())
                {
                    move_backward(alloc, it, end(), end() + (to - from));
                }
            
                copy(alloc, from, to, it);
                desc_.set_size(size() + static_cast< size_type >(to - from));

                checkvec();
            }

            value_type& operator[](size_type index)
            {
                assert(index < size());
                return *(begin() + index);
            }

            const value_type& operator[](size_type index) const 
            {
                assert(index < size());
                return *(begin() + index);
            }

        private:
            template < typename Allocator > void destroy(Allocator& alloc, iterator first, iterator last)
            {
                assert(last >= first);
                if constexpr (!std::is_trivially_destructible_v< T >)
                {
                    while (first != last)
                    {
                        std::allocator_traits< Allocator >::destroy(alloc, first++);
                    }
                }
            }
        
            template < typename Allocator > void move_backward(Allocator& alloc, iterator first, iterator last, iterator dest)
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
                        size_type uninitialized_count = static_cast< size_type >(std::min(last - first, dest - end()));
                        while (uninitialized_count--)
                        {
                            std::allocator_traits< Allocator >::construct(alloc, --dest, std::move(*--last));
                        }
                    }

                    std::move_backward(first, last, dest);
                }
            }

            template < typename Allocator, typename U > void copy(Allocator& alloc, U first, U last, iterator dest)
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
                        cnt = static_cast< size_type >(std::min(last - first, end() - dest));
                        std::copy(first, first + cnt, dest);
                    }

                    std::uninitialized_copy(first + cnt, last, dest + cnt);
                }
            }

            void checkvec()
            {
            #if defined(BTREE_CHECK_VECTOR_INVARIANTS)
                assert(end() - begin() == size());
                assert(size() <= capacity());

                vec_.assign(begin(), end());
                assert(std::is_sorted(vec_.begin(), vec_.end()));
            #endif
            }

            Descriptor desc_;

        #if defined(BTREE_CHECK_VECTOR_INVARIANTS)
            std::vector< T > vec_;
        #endif
        };

        // TODO: exception safety
        template < typename... Args > struct fixed_split_vector
            : std::tuple< Args... >
        {
        public:
            // TODO: extend to more than 2 fields
            // using value_type = std::tuple< typename Args::value_type&... >;

            using value_type = std::pair< typename Args::value_type&... >;
            using base_iterator = typename std::tuple_element_t< 0, std::tuple< Args... > >::iterator;
            using size_type = typename std::tuple_element_t< 0, std::tuple< Args... > >::size_type;
            using container_type = fixed_split_vector< Args... >;

            // TODO: to avoid end dereference, this should be a tuple of iterators
            struct iterator
            {
                using difference_type = std::ptrdiff_t;
                using value_type = value_type;
                using pointer = value_type*;
                using reference = value_type;
                using iterator_category = std::random_access_iterator_tag;

                iterator(container_type& container, base_iterator it)
                    : container_(container)
                    , it_(it)
                {}

                reference operator *()
                {
                    return container_[static_cast<size_type>(it_ - container_.base().begin())];
                }

                reference operator *() const
                {
                    return container_[static_cast<size_type>(it_ - container_.base().begin())];
                }

                // TODO: how should those +/- methods be implemented?
                difference_type operator - (iterator it) const { return it_ - it.it_; }

                iterator operator + (size_type n) const { return { container_, it_ + n }; }
                iterator operator - (size_type n) const { return { container_, it_ - n }; }

                bool operator == (const iterator& other) { return it_ == other.it_; }

            private:
                container_type& container_;
                base_iterator it_;
            };

            fixed_split_vector(Args... args)
                : std::tuple< Args... >(args...)
            {}

            template < typename Allocator, typename Ty > void emplace_back(Allocator& alloc, Ty&& value)
            {
                emplace(alloc, end(), std::forward< Ty >(value));
            }

            template < typename Allocator > iterator erase(Allocator& alloc, iterator index)
            {
                bool last = (index + 1 == end());
                erase_impl(alloc, index, sequence());
                return last ? end() : index;
            }

            template < typename Allocator > void erase(Allocator& alloc, iterator from, iterator to)
            {
                erase_impl(alloc, from, to, sequence());
            }

            size_type size() const { return base().size(); }
            size_type capacity() const { return base().capacity(); }
            bool empty() const { return base().size() == 0; }

            iterator begin() { return { *this, base().begin() }; }
            iterator end() { return { *this, base().end() }; }

            template < typename Allocator > void clear(Allocator& alloc)
            {
                clear_impl(alloc, sequence());
            }

            template < typename Allocator, typename Ty > void emplace(Allocator& alloc, iterator it, Ty&& value)
            {
                emplace_impl(alloc, it, std::forward< Ty >(value), sequence());
            }

            template < typename Allocator, typename U > void insert(Allocator& alloc, iterator it, U from, U to)
            {
                insert_impl(alloc, it, from, to, sequence());
            }

            auto operator[](size_type index)
            {
                return at_impl(index, sequence());
            }

            const auto operator[](size_type index) const
            {
                return at_impl(index, sequence());
            }

        private:
            static constexpr auto sequence() { return std::make_integer_sequence< size_t, sizeof...(Args) >(); }

            auto& base() { return std::get< 0 >(*this); }
            const auto& base() const { return std::get< 0 >(*this); }

            template < typename Allocator, size_t... Ids > void clear_impl(Allocator& alloc, std::integer_sequence< size_t, Ids... >)
            {
                (std::get< Ids >(*this).clear(alloc), ...);
            }

            template < typename Allocator, size_t... Ids > void erase_impl(Allocator& alloc, iterator index, std::integer_sequence< size_t, Ids... >)
            {
                auto offset = index - begin();
                (std::get< Ids >(*this).erase(alloc, std::get< Ids >(*this).begin() + offset), ...);
            }

            template < typename Allocator, size_t... Ids > void erase_impl(Allocator& alloc, iterator from, iterator to, std::integer_sequence< size_t, Ids... >)
            {
                auto first = from - begin();
                auto last = to - begin();
                (std::get< Ids >(*this).erase(alloc, std::get< Ids >(*this).begin() + first, std::get< Ids >(*this).begin() + last), ...);
            }

            template < typename Allocator, typename Ty, size_t... Ids > void emplace_impl(Allocator& alloc, iterator index, Ty&& value, std::integer_sequence< size_t, Ids... >)
            {
                auto offset = index - begin();
                (std::get< Ids >(*this).emplace(alloc, std::get< Ids >(*this).begin() + offset, std::get< Ids >(std::forward< Ty >(value))), ...);
            }

            template < typename Allocator, typename U, size_t... Ids > void insert_impl(Allocator& alloc, iterator index, std::move_iterator<U> from, std::move_iterator<U> to, std::integer_sequence< size_t, Ids... >)
            {
                auto offset = index - begin();
                (std::get< Ids >(*this).insert(
                    alloc, std::get< Ids >(*this).begin() + offset,
                    // TODO: this quite crudely expects continuous storage
                    std::make_move_iterator(&std::get< Ids >(*from)),
                    std::make_move_iterator(&std::get< Ids >(*to))
                ), ...);
            }

            template < size_t... Ids > auto at_impl(size_type index, std::integer_sequence< size_t, Ids... >)
            {
                // TODO: this dereferences end().
                return value_type(*(std::get< Ids >(*this).begin() + index)...);
            }

            template < size_t... Ids > auto at_impl(size_type index, std::integer_sequence< size_t, Ids... >) const
            {
                return value_type(*(std::get< Ids >(*this).begin() + index)...);
            }
        };

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
            template < typename Pair > static auto&& convert(Pair&& p) { return std::forward< Pair >(p); }

            template < typename T > static reference* reference_address(T&& p) 
            { 
                // TODO: this traits pair< Key&, Value& > as POD. Needed to return address of temporary pair< Key&, Value& > for iterator::operator ->().
                static thread_local std::aligned_storage_t< sizeof(reference) > storage;
                new (&storage) reference(std::forward< T >(p));
                return reinterpret_cast< reference* >(&storage); 
            }
        };

        template < typename Key > struct value_type_traits< Key, void >
        {
            typedef Key value_type;
            typedef const Key& reference;

            static const Key& get_key(const Key& p) { return p; }
            template < typename T > static auto&& convert(T&& p) { return p; }

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
                friend class container_type;

                iterator(value_node* n, node_size_type kindex)
                    : node_(n)
                    , kindex_(kindex)
                {}

                bool operator == (const iterator& rhs) const { return node_ == rhs.node_ && kindex_ == rhs.kindex_; }
                bool operator != (const iterator& rhs) const { return !(*this == rhs); }

                const reference operator*() const { return node_descriptor< value_node* >(node_).get_data()[kindex_]; }
                      reference operator*()       { return node_descriptor< value_node* >(node_).get_data()[kindex_]; }
                                      
                const std::remove_reference_t< reference >* operator->() const  { return value_type_traits_type::reference_address(node_descriptor< value_node* >(node_).get_data()[kindex_]); }
                      std::remove_reference_t< reference >* operator->()        { return value_type_traits_type::reference_address(node_descriptor< value_node* >(node_).get_data()[kindex_]); }

                iterator& operator++()
                {
                    auto node = node_descriptor< value_node* >(node_);
                    assert(node_);
                    assert(kindex_ <= node.get_keys().size());

                    if (++kindex_ == node.get_keys().size())
                    {
                    #if defined(BTREE_VALUE_NODE_LR)
                        if(node_->right)
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
                    ++*this;
                    return it;
                }

                iterator& operator--()
                {
                    assert(node_);
                    
                    auto node = node_descriptor< value_node* >(node_);
                    if (kindex_ == 0)
                    {                        
                    #if defined(BTREE_VALUE_NODE_LR)
                        assert(node_->left);
                        node_ = node_->left;
                    #else
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

            private:
                value_node* node_;
                node_size_type kindex_;
            };

            container_base()
                : root_()
                , first_node_()
                , last_node_()
                , size_()
                , depth_()
            {
                // TODO: default the constructor for release
            #if defined(_DEBUG)
                // Make sure the objects alias.
                value_node v;
                assert(&v == (node*)&v);
                internal_node n;
                assert(&n == (node*)&n);
            #endif
            }
                        
            container_base(container_type&& other)
                : root_(other.root_)
                , first_node_(other.first_node_)
                , last_node_(other.last_node_)
                , size_(other.size_)
                , depth_(other.depth_)
            {
                other.root_ = nullptr;
                other.first_node_ = other.last_node_ = nullptr;
                other.depth_ = other.size_ = 0;
            }

            container_base(const container_type&& other) = delete;

            container_type& operator = (container_type&& other) = default;

        #if defined(_DEBUG)
            ~container_base()
            {
                // TODO: default for release
                assert(empty());
            }
        #endif

            template < typename Allocator > void clear(Allocator& allocator)
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
        
            template < typename Allocator > std::pair< iterator, bool > insert(Allocator& allocator, const value_type& value)
            {
                return emplace_hint(allocator, nullptr, value);
            }

            template < typename Allocator, typename... Args > std::pair< iterator, bool > emplace(Allocator& allocator, Args&&... args)
            {
                return emplace_hint(allocator, nullptr, value_type(std::forward< Args >(args)...));
            }

            template < typename Allocator > std::pair< iterator, bool > insert(Allocator& allocator, iterator hint, const value_type& value)
            {
                return emplace_hint(allocator, &hint, value);
            }

            template < typename Allocator, typename It > void insert(Allocator& allocator, It begin, It end)
            {
                // TODO: hint, assume sorted range.
                while (begin != end)
                {
                    insert(allocator, *begin++);
                }
            }

            template < typename Allocator > void erase(Allocator& allocator, const key_type& key)
            {
                auto it = find(key);
                if (it != end())
                {
                    erase(allocator, it);
                }
            }

            template < typename Allocator > iterator erase(Allocator& allocator, iterator it)
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
            
        //private:
            value_node* hint_node(iterator* it) const
            {
                if (it)
                {
                    return *it == end() ? last_node_ : it->node_;
                }

                return nullptr;
            }

            template < typename Allocator, typename... Args > std::pair< iterator, bool > emplace_hint(Allocator& allocator, iterator* hint, Args&&... args)
            {
                value_type value(std::forward< Args >(args)...);

                if (!root_)
                {
                    auto root = allocate_node< value_node >(allocator);
                    root_ = first_node_ = last_node_ = root;
                    depth_ = 1;

                    return insert(allocator, root, 0, std::move(value));
                }
                                
                const auto& key = value_type_traits_type::get_key(value);

            #if defined(BTREE_VALUE_NODE_HINT)
                auto [n, nindex] = find_value_node(root_, hint_node(hint), key);
            #else
                auto [n, nindex] = find_value_node(root_, nullptr, key);
            #endif
                if (!full(n))
                {
                    return insert(allocator, n, nindex, std::move(value));
                }
                else
                {
                    if (n.get_parent())
                    {
                        std::tie(n, nindex) = rebalance_insert(allocator, depth_, n, nindex, key);
                        return insert(allocator, n, nindex, std::move(value));
                    }
                    else
                    {
                        std::tie(n, nindex) = rebalance_insert(allocator, depth_, n, key);
                        return insert(allocator, n, nindex, std::move(value));
                    }
                }
            }

            std::tuple< node_descriptor< value_node* >, node_size_type > find_value_node(node* n, value_node* hint, const key_type& key) const
            {
            #if defined(BTREE_VALUE_NODE_HINT)
                if (hint)
                {
                    if (hint->get_parent())
                    {
                        auto hkeys = hint->get_keys();
                        if (!hkeys.empty() && compare_lte(hkeys[0], key))
                        {                            
                            return { hint, get_index(desc(hint)) };
                        }
                    }
                    else
                    {
                        assert(hint == root_);
                        return { hint, 0 };
                    }
                }
            #endif                
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
                        nindex = static_cast< node_size_type >(kindex + !compare_lte(key, nkeys[kindex]));
                    }
                    else
                    {
                        nindex = nkeys.size();
                    }

                    n = in.get_children< node* >()[nindex];
                }

                return { node_cast<value_node*>(n), nindex };
            }

            iterator find(node* n, const key_type& key) const
            {
                auto [vn, vnindex] = find_value_node(n, nullptr, key);
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

            template < typename Allocator, typename T > std::pair< iterator, bool > insert(Allocator& allocator, node_descriptor< value_node* > n, node_size_type nindex, T&& value)
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
                    // TODO: move
                    n.get_data().emplace(allocator, n.get_data().begin() + kindex, value_type_traits_type::convert(std::forward< T >(value)));
                    ++size_;

                    return { iterator(n, kindex), true };
                }
            }

            template < typename Descriptor > auto find_key_index(const fixed_vector< Key, Descriptor >& keys, const key_type& key) const
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

                return static_cast< node_size_type >(index - keys.begin());
            }

            template < typename Node, typename Descriptor > static auto find_node_index(const fixed_vector< Node*, Descriptor >& nodes, const node* n)
            {
                // TODO: better search, this should use vector with thombstone so we can iterate over capacity() and unroll the iteration.
                // The code will count number of elements smaller than n (equals to index of n).
                for (decltype(nodes.size()) i = 0; i < nodes.size(); ++i)
                {
                    if (nodes[i] == n)
                    {
                        return i;
                    }
                }

                return nodes.size();
            }

            template< typename Allocator, typename Node > void remove_node(Allocator& allocator, size_type depth, node_descriptor< internal_node* > parent, const node_descriptor< Node* > n, node_size_type nindex, node_size_type kindex)
            {
                auto pchildren = parent.get_children< node* >();
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

                    deallocate_node(allocator, desc(node_cast< internal_node* >(root)));
                }
            }

            template < typename Node, typename Allocator > auto get_node_allocator(Allocator& allocator)
            {
                return std::allocator_traits< Allocator >::rebind_alloc< Node >(allocator);
            }

            template < typename Node, typename Allocator > Node* allocate_node(Allocator& allocator)
            {
                static_assert(!std::is_same_v<Node, node>);
            
                auto alloc = get_node_allocator< Node >(allocator);
                auto ptr = std::allocator_traits< decltype(alloc) >::allocate(alloc, 1);
                std::allocator_traits< decltype(alloc) >::construct(alloc, ptr);
                return ptr;
            }

            template < typename Allocator, typename Node > void deallocate_node(Allocator& allocator, node_descriptor< Node* > n)
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

            template < typename Allocator > std::tuple< node_descriptor< internal_node* >, key_type > split_node(Allocator& allocator, size_type depth, node_descriptor< internal_node* > lnode, node_size_type lindex, const key_type&)
            {
                assert(full(lnode));
                auto rnode = desc(allocate_node< internal_node >(allocator));

                auto lchildren = lnode.get_children< node* >();
                auto rchildren = rnode.get_children< node* >();
            
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

            template < typename Allocator > std::tuple< node_descriptor< value_node* >, key_type > split_node(Allocator& allocator, size_type, node_descriptor< value_node* > lnode, node_size_type lindex, const key_type& key)
            {
                assert(full(lnode));
                auto rnode = desc(allocate_node< value_node >(allocator));
                auto lkeys = lnode.get_keys();
                auto ldata = lnode.get_data();

            #if defined(BTREE_VALUE_NODE_APPEND)
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

            template < typename Allocator > void free_node(Allocator& allocator, node* n, size_type depth)
            {
                if (depth != depth_)
                {
                    auto in = desc(node_cast<internal_node*>(n));
                    auto nchildren = in.get_children< node* >();

                    for (auto child: nchildren)
                    {
                        free_node(allocator, child, depth + 1);
                    }

                    deallocate_node(allocator, in);
                }
                else
                {
                    deallocate_node(allocator, desc(node_cast< value_node* >(n)));
                }
            }

            template < typename Node > static Node first_node(node* n, size_type depth)
            {
                assert(n);
                while (--depth)
                {
                    n = desc(node_cast<internal_node*>(n)).get_children< node* >()[0];
                }

                return node_cast<Node>(n);
            }

            template < typename Node > static Node last_node(node* n, size_type depth)
            {
                assert(n);
                assert(depth > 0);
                while (--depth)
                {
                    auto children = desc(node_cast<internal_node*>(n)).get_children< node* >();
                    n = children[children.size() - 1];
                }

                return node_cast<Node>(n);
            }

            template < typename Allocator > bool share_keys(Allocator& allocator, size_t, node_descriptor< value_node* > target, node_size_type tindex, node_descriptor< value_node* > source, node_size_type sindex)
            {
                if (!source)
                {
                    return false;
                }

                auto sdata = source.get_data();
                auto tdata = target.get_data();
            
                if (sdata.size() > value_node::N && tdata.size() < 2 * value_node::N)
                {
                    auto pkeys = target.get_parent().get_keys();

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

                return false;
            }

            template < typename Allocator > bool share_keys(Allocator& allocator, size_type depth, node_descriptor< internal_node* > target, node_size_type tindex, node_descriptor< internal_node* > source, node_size_type sindex)
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

                    auto schildren = source.get_children< node* >();
                    auto tchildren = target.get_children< node* >();

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

            template < typename Allocator > bool merge_keys(Allocator& allocator, size_type depth, node_descriptor< value_node* > target, node_size_type tindex, node_descriptor< value_node* > source, node_size_type sindex)
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

            template < typename Allocator > bool merge_keys(Allocator& allocator, size_type depth, node_descriptor< internal_node* > target, node_size_type tindex, node_descriptor< internal_node* > source, node_size_type sindex)
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

                    auto schildren = source.get_children< node* >();
                    auto tchildren = target.get_children< node* >();
                
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

            template < typename Allocator, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_insert(Allocator& allocator, size_type depth, node_descriptor< Node* > n, const Key& key)
            {
                assert(full(n));
                if(n.get_parent())
                {                    
                    return rebalance_insert(allocator, depth, n, get_index(n), key);
                }
                else
                {
                    auto root = desc(allocate_node< internal_node >(allocator));
                    auto [p, splitkey] = split_node(allocator, depth, n, 0, key);

                    auto children = root.get_children< node* >();
                    children.emplace_back(allocator, n);
                    n.set_parent(root);
                    children.emplace_back(allocator, p);
                    p.set_parent(root);

                    if (root_ == last_node_)
                    {
                        last_node_ = node_cast< value_node* >(p);
                    }

                    auto cmp = !(compare_lte(key, splitkey));
                    root.get_keys().emplace_back(allocator, std::move(splitkey));

                    root_ = root;
                    ++depth_;

                    return { cmp ? p : n, cmp };
                }
            }

            template < typename Allocator, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_insert(Allocator& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex, const Key& key)
            {
                assert(full(n));
                assert(n.get_parent());

                auto parent_rebalance = full(n.get_parent());
                if(parent_rebalance)
                {
                    assert(depth > 1);
                    depth_check< true > dc(depth_, depth);
                    rebalance_insert(allocator, depth - 1, n.get_parent(), split_key(n.get_parent()));
                    assert(!full(n.get_parent()));
                }
                    
                // TODO: check that the key is still valid reference, it might not be after split
                auto [p, splitkey] = split_node(allocator, depth, n, nindex, key);
                    
                auto pchildren = n.get_parent().get_children< node* >();
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
                    last_node_ = node_cast< value_node* >(p.node());
                }

                auto cmp = !(compare_lte(key, splitkey));
                return { cmp ? p : n, nindex + cmp };
            }

            template < typename Allocator, typename Node > void rebalance_erase(Allocator& allocator, size_type depth, node_descriptor< Node* > n)
            {
                if (n.get_parent())
                {                    
                    rebalance_erase(allocator, depth, n, get_index(n));
                }
            }

            template < typename Allocator, typename Node > std::tuple< node_descriptor< Node* >, node_size_type > rebalance_erase(Allocator& allocator, size_type depth, node_descriptor< Node* > n, node_size_type nindex)
            {
                assert(n.get_parent());
                assert(n.get_keys().size() <= n.get_keys().capacity() / 2);

                {
                    auto [left, lindex] = get_left(n, nindex);
                    if (left && share_keys(allocator, depth, n, nindex, left, lindex))
                    {
                    #if defined(BTREE_VALUE_NODE_APPEND)
                        // TODO: investigate - right was 0, so possibly rigthtmost node append optimization?
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

        public:
            template< typename Node > static std::tuple< node_descriptor< Node >, node_size_type > get_right(node_descriptor< Node > n, node_size_type index, bool recursive = false)
            {
                node_descriptor< internal_node* > p(n.get_parent());
                size_type depth = 1;
                while (p)
                {
                    auto children = p.get_children< Node >();
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
            static std::tuple< node_descriptor< value_node* >, node_size_type > get_right(node_descriptor< value_node* > n, node_size_type index, bool)
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
                    auto children = p.get_children< Node >();
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
            static std::tuple< node_descriptor< value_node* >, node_size_type > get_left(node_descriptor< value_node* > n, node_size_type index, bool)
            {
                return { n.node()->left, n.node()->left ? get_index(n.node()->left) : 0 };
            }
        #endif

            template< typename Node > static node_size_type get_index(node_descriptor< Node > n)
            {
                auto index = node_size_type();
                
                if (n && n.get_parent())
                {
                    index = find_node_index(n.get_parent().get_children< node* >(), n);
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
                return reinterpret_cast< Node >(n);
            }

            bool compare_lte(const key_type& lhs, const key_type& rhs) const
            {
                return compare_(lhs, rhs) || !compare_(rhs, lhs);
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
    
                    auto children = desc(in).get_children< node* >();
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
        
            // TODO: get rid of compare, too
            Compare compare_;
        };
        
        template < typename Key, typename SizeType, SizeType N > struct internal_node : node
        {
            enum { N = N };

            internal_node() = default;

            uint8_t keys[(2 * N - 1) * sizeof(Key)];
            uint8_t children[2 * N * sizeof(node*)];
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

        template < typename Key, typename Value, typename SizeType, SizeType N, typename InternalNodeType > struct value_node : node
        {
            enum { N = N };

            value_node() = default;

            uint8_t keys[2 * N * sizeof(Key)];
            uint8_t values[2 * N * sizeof(Value)];
            InternalNodeType* parent;

        #if defined(BTREE_VALUE_NODE_LR)
            value_node* left;
            value_node* right;
        #endif
            SizeType size;
        };

        template < typename Key, typename Value, typename SizeType, SizeType N, typename InternalNodeType > struct node_descriptor< value_node< Key, Value, SizeType, N, InternalNodeType >* >
        {
            using value_node_type = value_node< Key, Value, SizeType, N, InternalNodeType >;
            using internal_node_type = InternalNodeType;
            using size_type = SizeType;

            using values_type = fixed_split_vector<
                fixed_vector< Key, keys_descriptor< value_node_type*, size_type, 2 * N > >,
                fixed_vector< Value, values_descriptor< value_node_type*, size_type, 2 * N > >
            >;

            node_descriptor(value_node_type* n)
                : node_(n)
            {}

            template < typename Allocator > void cleanup(Allocator& allocator) { get_data().clear(allocator); }

            auto get_keys() { return fixed_vector< Key, keys_descriptor< value_node_type*, size_type, 2 * N > >(node_); }
            auto get_keys() const { return fixed_vector< Key, keys_descriptor< value_node_type*, size_type, 2 * N > >(node_); }

            auto get_data() { return values_type(get_keys(), get_values()); }
            auto get_data() const { return values_type(get_keys(), get_values()); }

            node_descriptor< internal_node_type* > get_parent() { return node_->parent; }
            void set_parent(internal_node_type* p) { node_->parent = p; }

            bool operator == (const node_descriptor< value_node_type* >& other) const { return node_ == other.node_; }
            operator value_node_type* () { return node_; }
            value_node_type* node() { return node_; }

        private:
            auto get_values() { return fixed_vector< Value, values_descriptor< value_node_type*, size_type, 2 * N > >(node_); }
            auto get_values() const { return fixed_vector< Value, values_descriptor< value_node_type*, size_type, 2 * N > >(node_); }

            value_node_type* node_;
        };

        template < typename Key, typename SizeType, SizeType N, typename InternalNodeType > struct value_node< Key, void, SizeType, N, InternalNodeType > : node
        {
            enum { N = N };

            value_node() = default;

            uint8_t keys[2 * N * sizeof(Key)];
            InternalNodeType* parent;
        #if defined(BTREE_VALUE_NODE_LR)
            value_node* left;
            value_node* right;
        #endif
            SizeType size;
        };

        template < typename Key, typename SizeType, SizeType N, typename InternalNodeType > struct node_descriptor< value_node< Key, void, SizeType, N, InternalNodeType >* >
        {
            using value_node_type = value_node< Key, void, SizeType, N, InternalNodeType >;
            using internal_node_type = InternalNodeType;
            using size_type = SizeType;

            node_descriptor(value_node_type* n)
                : node_(n)
            {}

            template < typename Allocator > void cleanup(Allocator& allocator) { get_keys().clear(allocator); }

            auto get_keys() { return fixed_vector< Key, keys_descriptor< value_node_type*, size_type, 2 * N > >(node_); }
            auto get_keys() const { return fixed_vector< Key, keys_descriptor< value_node_type*, size_type, 2 * N > >(node_); }

            auto get_data() { return get_keys(); }
            auto get_data() const { return get_keys(); }

            node_descriptor< internal_node_type* > get_parent() { return node_->parent; }
            void set_parent(internal_node_type* p) { node_->parent = p; }

            bool operator == (const node_descriptor< value_node_type* >& other) const { return node_ == other.node_; }
            operator value_node_type* () { return node_; }
            value_node_type* node() { return node_; }

        private:
            value_node_type* node_;
        };
    }

    template <
        typename Key,
        typename Compare = std::less< Key >,
        typename Allocator = std::allocator< Key >,
        typename NodeSizeType = uint8_t,
        NodeSizeType N = detail::node_dimension< uint8_t, Key >::value,
        typename InternalNodeType = detail::internal_node< Key, NodeSizeType, N >,
        typename ValueNodeType = detail::value_node< Key, void, NodeSizeType, N, InternalNodeType >
    > class set_base
        : public detail::container_base< Key, void, Compare, Allocator, NodeSizeType, InternalNodeType, ValueNodeType >
    {
        using base_type = detail::container_base< Key, void, Compare, Allocator, NodeSizeType, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
    };

    template < 
        typename Key, 
        typename Compare = std::less< Key >, 
        typename Allocator = std::allocator< Key >,
        typename NodeSizeType = uint8_t,
        NodeSizeType N = detail::node_dimension< uint8_t, Key >::value,
        typename InternalNodeType = detail::internal_node< Key, NodeSizeType, N >,
        typename ValueNodeType = detail::value_node< Key, void, NodeSizeType, N, InternalNodeType >
    > class set
        : public set_base< Key, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >
    {
        using base_type = set_base< Key, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
        using value_type = typename base_type::value_type;
        using iterator = typename base_type::iterator;

        set(Allocator& allocator = Allocator())
            : allocator_(allocator)
        {}
              
        set(set< Key, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >&& other) = default;

        ~set()
        {
            clear();
        }

        std::pair< iterator, bool > insert(const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(allocator_, nullptr, value));
        }

        template < typename It > void insert(It begin, It end)
        {
            BTREE_CHECK(base_type::insert(allocator_, begin, end));
        }

        void erase(const value_type& key)
        {
            BTREE_CHECK(base_type::erase(allocator_, key));
        }

        iterator erase(iterator it)
        {
            BTREE_CHECK_RETURN(base_type::erase(allocator_, it));
        }

        void clear()
        {
            // TODO: missing test
            BTREE_CHECK(base_type::clear(allocator_));
        }

    private:
        allocator_type allocator_;
    };
    
    template <
        typename Key,
        typename Value,
        typename Compare = std::less< Key >,
        typename Allocator = std::allocator< std::pair< Key, Value > >,
        typename NodeSizeType = uint8_t,
        NodeSizeType N = detail::node_dimension< uint8_t, Key >::value,
        typename InternalNodeType = detail::internal_node< Key, NodeSizeType, N >,
        typename ValueNodeType = detail::value_node< Key, Value, NodeSizeType, N, InternalNodeType >
    > class map_base
        : public detail::container_base< Key, Value, Compare, Allocator, NodeSizeType, InternalNodeType, ValueNodeType >
    {
        using base_type = detail::container_base< Key, Value, Compare, Allocator, NodeSizeType, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
    };

    template <
        typename Key, 
        typename Value, 
        typename Compare = std::less< Key >, 
        typename Allocator = std::allocator< std::pair< Key, Value > >,
        typename NodeSizeType = uint8_t,
        NodeSizeType N = detail::node_dimension< uint8_t, Key >::value,
        typename InternalNodeType = detail::internal_node< Key, NodeSizeType, N >,
        typename ValueNodeType = detail::value_node< Key, Value, NodeSizeType, N, InternalNodeType >
    > class map
        : public map_base< Key, Value, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >
    {
        using base_type = map_base< Key, Value, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
        using value_type = typename base_type::value_type;
        using iterator = typename base_type::iterator;

        map(Allocator& allocator = Allocator())
            : allocator_(allocator)
        {}

        map(map< Key, Value, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >&& other) = default;

        ~map()
        {
            clear();
        }

        std::pair< iterator, bool > insert(const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(allocator_, nullptr, value));
        }

        template < typename It > void insert(It begin, It end)
        {
            BTREE_CHECK_RETURN(base_type::insert(allocator_, begin, end));
        }

        void erase(const typename value_type::first_type& key)
        {
            BTREE_CHECK(base_type::erase(allocator_, key));
        }

        void clear()
        {
            // TODO: missing test
            BTREE_CHECK(base_type::clear(allocator_));
        }

    private:
        allocator_type allocator_;
    };
}