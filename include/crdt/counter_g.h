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

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        container_type values_;
    };
}