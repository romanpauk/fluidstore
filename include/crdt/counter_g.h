#pragma once

#include <sqlstl/algorithm.h>
#include <crdt/allocator.h>
#include <algorithm>

namespace crdt
{
    template < typename T, typename Node, typename Traits > class counter_g
    {
        typedef typename Traits::template Map< Node, T > container_type;
        template < typename T, typename Node, typename Traits > friend class counter_g;

    public:
        typedef typename Traits::template Allocator<void> allocator_type;
    
        counter_g(allocator_type allocator)
            : values_(allocator_type(allocator, "values"))
            , allocator_(allocator)
        {}

        void increment(const Node& node, T value)
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

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        container_type values_;
    };

    template < typename T, typename Node, typename StateTraits, typename DeltaTraits > class counter_g_delta
        : public delta_crdt_base
    {
        typedef counter_g< T, Node, StateTraits > state_container_type;
        typedef counter_g< T, Node, DeltaTraits > delta_container_type;

    public:
        counter_g_delta(state_container_type& state_container)
            : state_container_(state_container)
        {}

        void increment(const Node& node, T value)
        {
            // TODO: counter_g itself needs to be thread-safe
            // And operations from the same node needs to be synchronized.
            
            delta_container_.increment(node, state_container_.value(node) + value);
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