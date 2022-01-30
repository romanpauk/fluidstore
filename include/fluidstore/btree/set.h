#pragma once

#include <fluidstore/btree/detail/container.h>

namespace btree
{
    namespace detail
    {
        template < typename Key, typename SizeType, SizeType Dim, typename InternalNodeType > struct value_node< Key, void, SizeType, Dim, InternalNodeType > : node
        {
            enum { N = Dim };
                        
            alignas(Key) uint8_t keys[2 * N * sizeof(Key)];
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
            
            using key_array_descriptor = array_descriptor< array_size_ref< size_type >, 2 * N >;

            node_descriptor(value_node_type* n)
                : node_(n)
            {}

            template < typename Allocator > void cleanup(Allocator& allocator) { get_keys().clear(allocator); }

            auto get_keys() { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }
            auto get_keys() const { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }

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

        set_base() = default;
        set_base(Allocator&) {}
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
        , private detail::compressed_base< Allocator >
        , private detail::compressed_base< Compare >
    {
        using base_type = set_base< Key, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
        using value_type = typename base_type::value_type;
        using size_type = typename base_type::size_type;
        using iterator = typename base_type::iterator;
        using const_iterator = typename base_type::const_iterator;

        // TODO: constructors 
        
        set() = default;

        explicit set(const allocator_type& allocator)
            : detail::compressed_base< Allocator >(allocator)
        {}

        set(set< Key, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >&& other) = default;

        ~set()
        {
            clear();
        }

        void clear() noexcept
        {
            BTREE_CHECK(base_type::clear(get_allocator()));
        }

        template < typename... Args > std::pair< iterator, bool > emplace(Args&&... args)
        {
            BTREE_CHECK_RETURN(base_type::emplace(get_allocator(), std::forward< Args >(args)...));
        }

        size_type erase(const value_type& key)
        {
            BTREE_CHECK_RETURN(base_type::erase(get_allocator(), key));
        }
                
        iterator erase(const_iterator it)
        {
            BTREE_CHECK_RETURN(base_type::erase(get_allocator(), it));
        }

        // TODO
        //iterator  erase(const_iterator first, const_iterator last);

        std::pair< iterator, bool > insert(const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace(get_allocator(), value));
        }

        std::pair< iterator, bool > insert(value_type&& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace(get_allocator(), std::move(value)));
        }

        iterator insert(iterator hint, const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(get_allocator(), hint, value));
        }

        iterator insert(iterator hint, value_type&& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(get_allocator(), hint, std::move(value)));
        }

        template < typename It > void insert(It begin, It end)
        {
            BTREE_CHECK(base_type::insert(get_allocator(), begin, end));
        }

        // TODO
        // void insert(initializer_list<value_type> il);    
                
    private:
        auto get_allocator() const { return detail::compressed_base< Allocator >::get(); }
        auto key_comp() const { return detail::compressed_base< Compare >::get(); }
    };
}