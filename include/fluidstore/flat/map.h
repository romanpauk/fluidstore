#pragma once

#include <fluidstore/flat/set.h>

namespace crdt::flat
{
    template < typename K, typename V > class map_base
    {
        struct node : std::pair< const K, V >
        {
            template < typename Kt, typename Vt > node(Kt&& k, Vt&& v)
                : std::pair< const K, V >(std::forward< Kt >(k), std::forward< Vt >(v))
            {}

            node(node&& node)
                : pair< const K, V >(std::move(node))
            {}

            bool operator < (const node& other) const { return first < other.first; }
            bool operator > (const node& other) const { return first > other.first; }
            bool operator == (const node& other) const { return first == other.first; }
        };

    public:
        using set_type = set_base< node >;
        using size_type = typename set_type::size_type;
        using iterator = typename set_type::iterator;       
        using node_type = node;

        map_base()
        {}

        map_base(map_base< K, V >&& other)
            : data_(std::move(other.data_))
        {}

        template< typename Allocator > void emplace(Allocator& allocator, const K& k, const V& v)
        {
            data_.emplace(allocator, node(k, v));
        }

        auto find(const K& key)
        {
            // TODO:
            //return data_.find(node(key, V()));

            for (auto it = data_.begin(); it != data_.end(); ++it)
            {
                if (it->first == key)
                {
                    return it;
                }
            }

            return end();
        }

        template < typename Allocator > void erase(Allocator& allocator, const K& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase(allocator, it);
            }
        }

        template < typename Allocator > void erase(Allocator& allocator, iterator it)
        {
            data_.erase(allocator, it);
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

    /*
    template < typename T, typename Allocator > class set: private set_base< T >
    {
    public:
        //using allocator_type = typename std::allocator_traits< Allocator >::template rebind< T >;
        using typename set_base< T >::iterator;

        set(Allocator& allocator)
            : allocator_(allocator)
        {}

    private:
        Allocator& allocator_;
    };
    */
}