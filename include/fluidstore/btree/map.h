#pragma once

#include <fluidstore/btree/detail/container.h>

namespace btree
{
    namespace detail 
    {
        template < typename Key, typename Value, typename SizeType, SizeType Dim, typename InternalNodeType > struct value_node : node
        {
            enum { N = Dim };

            alignas(Key) uint8_t keys[2 * N * sizeof(Key)];
            alignas(Value) uint8_t values[2 * N * sizeof(Value)];
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

            using key_array_descriptor = array_descriptor< array_size_ref< size_type >, 2 * N >;
            using value_array_descriptor = array_descriptor< array_size_copy< size_type >, 2 * N >;

            using values_type = fixed_split_vector<
                fixed_vector< Key, key_array_descriptor >,
                fixed_vector< Value, value_array_descriptor >
            >;

            node_descriptor(value_node_type* n)
                : node_(n)
            {}

            template < typename Allocator > void cleanup(Allocator& allocator) { get_data().clear(allocator); }

            auto get_keys() { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }
            auto get_keys() const { return fixed_vector< Key, key_array_descriptor >(node_->keys, node_->size); }

            auto get_data() { return values_type(get_keys(), get_values()); }
            auto get_data() const { return values_type(get_keys(), get_values()); }

            node_descriptor< internal_node_type* > get_parent() { return node_->parent; }
            void set_parent(internal_node_type* p) { node_->parent = p; }

            bool operator == (const node_descriptor< value_node_type* >& other) const { return node_ == other.node_; }
            operator value_node_type* () { return node_; }
            value_node_type* node() { return node_; }

        private:
            auto get_values() { return fixed_vector< Value, value_array_descriptor >(node_->values, node_->size); }
            auto get_values() const { return fixed_vector< Value, value_array_descriptor >(node_->values, node_->size); }

            value_node_type* node_;
        };
    }

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

        map_base() = default;
        template < typename AllocatorT > map_base(AllocatorT&&) {}
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
        , private detail::compressed_base< Allocator >
        , private detail::compressed_base< Compare >
    {
        using base_type = map_base< Key, Value, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >;

    public:
        using allocator_type = Allocator;
        using value_type = typename base_type::value_type;
        using key_type = typename base_type::key_type;
        using iterator = typename base_type::iterator;
        using const_iterator = typename base_type::const_iterator;
        using size_type = typename base_type::size_type;

        map() = default;

        template < typename AllocatorT > map(AllocatorT&& allocator)
            : detail::compressed_base< Allocator >(std::forward< AllocatorT >(allocator))
        {}

        map(map< Key, Value, Compare, Allocator, NodeSizeType, N, InternalNodeType, ValueNodeType >&& other) = default;

        ~map()
        {
            clear();
        }

        iterator find(const key_type& key) { return base_type::find(this->key_comp(), key); }
        const_iterator find(const key_type& key) const { return base_type::find(this->key_comp(), key); }

        std::pair< iterator, bool > insert(const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace(this->get_allocator(), this->key_comp(), value));
        }

        std::pair< iterator, bool > insert(value_type&& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace(this->get_allocator(), this->key_comp(), std::move(value)));
        }

        std::pair< iterator, bool > insert(iterator hint, const value_type& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(this->get_allocator(), this->key_comp(), hint, value));
        }

        std::pair< iterator, bool > insert(iterator hint, value_type&& value)
        {
            BTREE_CHECK_RETURN(base_type::emplace_hint(this->get_allocator(), this->key_comp(), hint, std::move(value)));
        }

        template < typename It > void insert(It begin, It end)
        {
            BTREE_CHECK(base_type::insert(this->get_allocator(), this->key_comp(), begin, end));
        }

        // TODO
        // void insert(initializer_list<value_type> il);

        size_type erase(const typename value_type::first_type& key)
        {
            BTREE_CHECK_RETURN(base_type::erase(this->get_allocator(), this->key_comp(), key));
        }

        iterator erase(const_iterator it)
        {
            BTREE_CHECK_RETURN(base_type::erase(this->get_allocator(), this->key_comp(), it));
        }

        void clear() noexcept
        {
            // TODO: missing test
            BTREE_CHECK(base_type::clear(this->get_allocator()));
        }

        Value& operator[](const Key& key)
        {
            // TODO: missing test
            // TODO: missing BTREE_CHECK_RETURN as it requires move
            return (*base_type::emplace(this->get_allocator(), this->key_comp(), key, Value()).first).second;
        }

        Value& operator[](Key&& key)
        {
            // TODO: missing test
            // TODO: missing BTREE_CHECK_RETURN as it requires move
            return (*base_type::emplace(this->get_allocator(), this->key_comp(), std::move(key), Value()).first).second;
        }

    private:
        auto get_allocator() const { return detail::compressed_base< Allocator >::get(); }
        auto key_comp() const { return detail::compressed_base< Compare >::get(); }
    };
}