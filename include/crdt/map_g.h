#pragma once

#include <crdt/set_g.h>

#include <type_traits>
#include <optional>

namespace crdt
{
    template < typename Key, typename Value, typename Traits, bool IsStandardLayout = std::is_standard_layout_v< Value > > class map_g_base;

    template < typename Key, typename Value, typename Traits > class map_g_base< Key, Value, Traits, false >
    {
        typedef typename Traits::template Map< Key, Value > container_type;
        typedef typename container_type::allocator_type allocator_type;
        
        template < typename It > struct iterator_base
        {
            iterator_base(const iterator_base< It >&) = delete;
            iterator_base& operator = (const iterator_base< It >&) = delete;

            iterator_base(
                map_g_base< Key, Value, Traits >& map,
                typename It&& it
            )
                : it_(std::move(it))
                , map_(map)
            {}

            iterator_base(
                map_g_base< Key, Value, Traits >& map,
                typename It&& it,
                Value&& value
            )
                : map_(map)
                , it_(std::move(it))
            {}

            iterator_base(iterator_base< It >&& other)
                : map_(other.map_)
                , it_(std::move(other.it_))
            {}

            auto& operator*() { return *it_; }
            auto operator->() { return &this->operator*(); }

            bool operator == (const iterator_base< It >& other) const { return it_ == other.it_; }
            bool operator != (const iterator_base< It >& other) const { return it_ != other.it_; }

            iterator_base< It >& operator++() { ++it_; return *this; }

        private:
            map_g_base< Key, Value, Traits >& map_;
            typename It it_;
        };

    public:
        typedef iterator_base< typename container_type::iterator > iterator;
        typedef iterator_base< typename container_type::const_iterator > const_iterator;
        
        template < typename Allocator > map_g_base(Allocator&& allocator)
            : allocator_(std::forward< Allocator >(allocator))
            , values_(allocator_type(allocator_))
        {}

        iterator begin() { return iterator(*this, values_.begin()); }
        const_iterator begin() const { return const_iterator(const_cast< map_g_base< Key, Value, Traits >& >(*this), values_.begin()); }

        iterator end() { return iterator(*this, values_.end()); }
        const_iterator end() const { return const_iterator(const_cast< map_g_base< Key, Value, Traits >& >(*this), values_.end()); }

        size_t size() const { return values_.size(); }

        template < typename K > auto operator[](K&& key) -> decltype(this->values_.operator[](std::forward< K >(key)))
        {
            return values_.operator[](std::forward< K >(key));
        }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto pairb = values_.emplace(std::forward< K >(key), std::forward< V >(value));
            return pairb;
        }

        template < typename K > iterator find(K&& key) { return iterator(*this, values_.find(std::forward< K >(key))); }

        template < typename Map > void merge(const Map& other)
        {
            for (auto&& value : other)
            {
                merge(value.first, value.second);
            }
        }

        template < typename K, typename V > void merge(K&& key, V&& value)
        {
            auto pairb = values_.emplace(std::forward< K >(key), value);
            pairb.first->second.merge(std::forward< V >(value));
        }

        template < typename K > Value get_value(K&& key) const
        {
            return Value(allocator_type(allocator_, std::to_string(key)));
        }

        allocator_type allocator_;
        container_type values_;
    };

    template < typename Key, typename Value, typename Traits > class map_g
        : public map_g_base< Key, Value, Traits >
    {
    public:
        template < typename Allocator > map_g(Allocator&& allocator)
            : map_g_base< Key, Value, Traits >(std::forward< Allocator >(allocator))
        {}
    };
}
