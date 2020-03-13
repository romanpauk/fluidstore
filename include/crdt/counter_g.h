#pragma once

#include <sqlstl/algorithm.h>
#include <crdt/Allocator.h>

namespace crdt
{
    template < typename T, typename Node, typename Traits > class counter_g
    {
    public:
        counter_g(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : values_(allocator.create_container< typename Traits::template Map< Node, T, typename Traits::Allocator > >(name))
        {}

        void add(const Node& node, T value)
        {
            values_[node] += value;
        }

        T value(const Node& node) const
        {
            auto it = values_.find(node);
            if (it != values_.end())
            {
                return it->second;
            }

            return T();
        }

        T value() const
        {
            T result = 0;
            return sqlstl::accumulate(values_.begin(), values_.end(), result, [](T init, auto&& p) { return init + p.second; });
        }

        template < typename Counter > void merge(const Counter& other)
        {
            for (auto&& otherValue : other.values_)
            {
                auto&& localValue = values_[otherValue.first];
                localValue = std::max((T)localValue, otherValue.second);
            }
        }

        template < typename Counter > bool operator == (const Counter& other) const
        {
            return values_ == other.values_;
        }

        // private:
        typename Traits::template Map< Node, T, typename Traits::Allocator > values_;
    };

    template < typename T, typename Node, typename DeltaTraits, typename StateTraits = DeltaTraits > class delta_counter_g
        : public counter_g< T, Node, StateTraits >
    {
    public:
        delta_counter_g(typename StateTraits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : counter_g< T, Node, StateTraits >(allocator, name)
        {}

        void add(const Node& node, T value)
        {
            counter_g< T, Node, DeltaTraits > delta;
            delta.add(node, value);
            this->merge(delta);
        }
    };
}