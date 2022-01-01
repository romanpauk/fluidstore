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
        map_base(Allocator&) {}
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

        Value& operator[](const Key& key)
        {
            // TODO: missing test
            // TODO: missing BTREE_CHECK_RETURN as it requires move
            return (*base_type::emplace(allocator_, key, Value()).first).second;
        }

    private:
        allocator_type allocator_;
    };
}