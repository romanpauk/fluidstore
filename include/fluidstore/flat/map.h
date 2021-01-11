#pragma once

#include <fluidstore/flat/set.h>

namespace crdt::flat
{
    template < typename K, typename V > class map_base
    {
        struct node
        {            
            bool operator < (const node& other) const { return first < other.first; }
            bool operator <= (const node& other) const { return first <= other.first; }
            bool operator > (const node& other) const { return first > other.first; }
            bool operator >= (const node& other) const { return first >= other.first; }
            bool operator == (const node& other) const { return first == other.first; }
            bool operator != (const node& other) const { return first != other.first; }

            bool operator == (const K& other) const { return first == other; }
            bool operator < (const K& other) const { return first < other; }

            const K first;
            V second;
        };

    public:
        using set_type = set_base< node >;
        using size_type = typename set_type::size_type;
        using iterator = typename set_type::iterator;   
        using const_iterator = typename set_type::const_iterator;
        using value_type = typename set_type::value_type;
        using node_type = node;

        map_base()
        {}

        map_base(map_base< K, V >&& other)
            : data_(std::move(other.data_))
        {}

        template< typename Allocator, typename Kt, typename Vt > auto emplace(Allocator& allocator, Kt&& k, Vt&& v)
        {
            return data_.emplace(allocator, node{ std::forward< Kt >(k), std::forward< Vt >(v) });
        }

        auto find(const K& key)
        {
            return data_.find_impl(key);
        }

        template < typename Allocator > void erase(Allocator& allocator, const K& key)
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
        set_type data_;
    };

    template < typename K, typename V, typename Allocator > class map : private map_base< K, V >
    {
        using map_base_type = map_base< K, V >;

    public:
        using allocator_type = Allocator;

        using typename map_base_type::iterator;
        using typename map_base_type::const_iterator;
        using typename map_base_type::value_type;

        map(allocator_type& allocator)
            : allocator_(allocator)
        {}

        map(map< K, V, Allocator >&& other)
            : map_base< K, V >(std::move(other))
            , allocator_(std::move(other.allocator_))
        {}

        ~map() 
        { 
            clear(allocator_); 
        }

        V& operator [](const K& key)
        {
            // TODO: this is crazy
            std::aligned_storage_t< sizeof(V) > v;
            auto allocator = std::allocator_traits< Allocator >::rebind_alloc< V >(allocator_);
            std::allocator_traits< decltype(allocator) >::construct(allocator, reinterpret_cast<V*>(&v));
            return emplace(allocator_, key, std::move(*reinterpret_cast< V* >(&v))).first->second;
        }

        template< typename Key, typename... Args > std::pair< iterator, bool > try_emplace(Key&& key, Args&&... args)
        {
            std::aligned_storage_t< sizeof(V) > v;
            auto allocator = std::allocator_traits< Allocator >::rebind_alloc< V >(allocator_);
            std::allocator_traits< decltype(allocator) >::construct(allocator, reinterpret_cast< V* >(&v), std::forward< Args >(args)...);
            return emplace(allocator_, std::forward< Key >(key), std::move(*reinterpret_cast<V*>(&v)));
        }

        auto erase(const K& value)
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