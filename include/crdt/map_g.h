#pragma once

#include <crdt/set_g.h>

#include <type_traits>
#include <optional>

namespace crdt
{
    template < typename Key, typename Value, typename Traits > class map_g
    {
    public:
        typedef typename Traits::template Allocator< void > allocator_type;

    private:
        typedef typename Traits::template Map< Key, Value > container_type;
        
        template < typename It > struct iterator_base
        {
            iterator_base(const iterator_base< It >&) = delete;
            iterator_base& operator = (const iterator_base< It >&) = delete;

            iterator_base(
                map_g< Key, Value, Traits >& map,
                typename It&& it
            )
                : it_(std::move(it))
                , map_(map)
            {}

            iterator_base(
                map_g< Key, Value, Traits >& map,
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
            map_g< Key, Value, Traits >& map_;
            typename It it_;
        };

        // Moved here so operator[] compiles
        allocator_type allocator_;
        container_type values_;

    public:
        typedef iterator_base< typename container_type::iterator > iterator;
        typedef iterator_base< typename container_type::const_iterator > const_iterator;
        
        map_g(allocator_type allocator)
            : allocator_(allocator)
            , values_(allocator_type(allocator_, "values"))
        {}

        iterator begin() { return iterator(*this, values_.begin()); }
        const_iterator begin() const { return const_iterator(const_cast< map_g< Key, Value, Traits >& >(*this), values_.begin()); }

        iterator end() { return iterator(*this, values_.end()); }
        const_iterator end() const { return const_iterator(const_cast< map_g< Key, Value, Traits >& >(*this), values_.end()); }

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

    private:
        template < typename K, typename V > void merge(K&& key, V&& value)
        {
            values_[key].merge(value);
            //auto pairb = values_.emplace(std::forward< K >(key), value);
            //pairb.first->second.merge(std::forward< V >(value));
        }       
    };

    template < typename T, typename Node, typename StateTraits, typename DeltaTraits > class delta_map_g
        : public delta_crdt_base
    {
        typedef map_g< T, Node, StateTraits > state_container_type;
        typedef map_g< T, Node, DeltaTraits > delta_container_type;

    public:

        delta_map_g(state_container_type& state_container)
            : state_container_(state_container)
        {}

        void apply() override
        {

        }

        void commit() override
        {
            state_container_.merge(delta_container_);
        }

    private:
        state_container_type& state_container_;
        delta_container_type delta_container_;
    };
}

