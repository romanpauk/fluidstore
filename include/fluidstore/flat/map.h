#pragma once

#include <fluidstore/flat/set.h>

namespace crdt::flat
{
    template < typename Key, typename Value > struct map_node
    {
        bool operator < (const map_node< Key, Value >& other) const { return first < other.first; }
        bool operator <= (const map_node< Key, Value >& other) const { return first <= other.first; }
        bool operator > (const map_node< Key, Value >& other) const { return first > other.first; }
        bool operator >= (const map_node< Key, Value >& other) const { return first >= other.first; }
        bool operator == (const map_node< Key, Value >& other) const { return first == other.first; }
        bool operator != (const map_node< Key, Value >& other) const { return first != other.first; }

        bool operator == (const Key& other) const { return first == other; }
        bool operator < (const Key& other) const { return first < other; }

        const Key first;
        Value second;
    };

    template < typename Key, typename Value, typename Node = map_node< Key, Value > > class map_base
    {
    public:
        using vector_type = vector_base< Node >;
        using value_type = typename vector_type::value_type;
        using size_type = typename vector_type::size_type;
        using iterator = typename vector_type::iterator;
        using const_iterator = typename vector_type::const_iterator;
        using node_type = Node;

        map_base()
        {}

        map_base(map_base< Key, Value, Node >&& other)
            : data_(std::move(other.data_))
        {}

        template< typename Allocator, typename... Args > std::pair< iterator, bool > emplace(Allocator& allocator, Args&&... args)
        {
            Node node{ std::forward< Args >(args)... };

            if (!data_.empty() && !(*--end() < node))
            {
                auto it = this->lower_bound_impl(node);
                if (it != end())
                {
                    if (*it == node)
                    {
                        return { it, false };
                    }
                    else
                    {
                        return { data_.emplace(allocator, it, std::move(node)), true };
                    }
                }
            }

            return { data_.emplace(allocator, data_.end(), std::move(node)), true };
        }

        template < typename Kt > auto find(const Kt& key)
        {
            return find_impl(key);
        }

        template< typename Ty > auto find_impl(const Ty& key)
        {
            auto it = this->lower_bound_impl(key);
            if (it != end() && *it == key)
            {
                return it;
            }

            return end();
        }

        template < typename Allocator, typename Kt > void erase(Allocator& allocator, const Kt& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase(allocator, it);
            }
        }

        template < typename Allocator > auto erase(Allocator& allocator, iterator it)
        {
            return data_.erase(allocator, it);
        }

        iterator lower_bound(const Key& key)
        {
            return lower_bound_impl(key);
        }

        template< typename Ty > auto lower_bound_impl(const Ty& value)
        {
            return std::lower_bound(data_.begin(), data_.end(), value);
        }

        auto begin() { return data_.begin(); }
        auto begin() const { return data_.begin(); }
        auto end() { return data_.end(); }
        auto end() const { return data_.end(); }

        auto empty() const { return data_.empty(); }
        auto size() const { return data_.size(); }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            data_.clear(allocator);
        }

    private:
        vector_type data_;
    };

    template < typename Key, typename Value, typename Allocator, typename Node = map_node< Key, Value > > class map 
        : private map_base< Key, Value, Node >
    {
        using map_base_type = map_base< Key, Value, Node >;

    public:
        using allocator_type = Allocator;

        using typename map_base_type::iterator;
        using typename map_base_type::const_iterator;
        using typename map_base_type::value_type;

        map(allocator_type& allocator)
            : allocator_(allocator)
        {}

        map(map< Key, Value, Allocator >&& other)
            : map_base_type(std::move(other))
            , allocator_(std::move(other.allocator_))
        {}

        ~map() 
        { 
            clear(allocator_); 
        }

        auto erase(const Key& value)
        {
            return erase(allocator_, value);
        }

        auto erase(iterator it)
        {
            return map_base_type::erase(allocator_, it);
        }

        using map_base_type::begin;
        using map_base_type::end;
        using map_base_type::size;
        using map_base_type::empty;
        using map_base_type::find;

    private:
        // TODO: should be ref, or I need a ref-wrapper.
        allocator_type allocator_;
    };
}