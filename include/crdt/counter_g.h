#pragma once

#include <sqlstl/algorithm.h>
#include <crdt/allocator.h>
#include <crdt/map_g.h>

#include <algorithm>

namespace crdt
{
/*
    template < typename Node, typename T, typename Traits > class counter_g
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

        void increment(typename container_type::iterator& it, T value)
        {
            it->second += value;
        }

        typename container_type::iterator value(const Node& node)
        {
            return values_.find(node);
        }

        typename container_type::iterator end()
        {
            return values_.end();
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

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        container_type values_;
    };
*/

    template < typename Node, typename T, typename Traits > class counter_g
    {
        typedef map_g< Node, T, Traits > container_type;
        template < typename Node, typename T, typename Traits > friend class counter_g;

    public:
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_g(allocator_type allocator)
            : container_(allocator_type(allocator, "values"))
            , allocator_(allocator)
        {}

        void update(const Node& node, T value)
        {
            assert(value > 0);
            auto pairb = container_.emplace(node, value);
            if (!pairb.second)
            {
                update(pairb.first, value);
            }
        }

        void update(typename container_type::iterator& it, T value)
        {
            assert(value > 0);
            it->second += value;
        }

        typename container_type::iterator find(const Node& node)
        {
            return container_.find(node);
        }

        typename container_type::iterator end()
        {
            return container_.end();
        }

        T value() const
        {
            T result = 0;
            return sqlstl::accumulate(container_.begin(), container_.end(), result, [](T init, auto&& p) { return init + p.second; });
        }

        template < typename Counter > void merge(const Counter& other)
        {
            for (auto&& otherValue : other.container_)
            {
                auto&& localValue = container_[otherValue.first];
                localValue = std::max((T)localValue, otherValue.second);
            }
        }

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        container_type container_;
    };

    template < typename Node, typename T, typename Traits > class counter_g_delta
    {
    public:
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_g_delta(allocator_type allocator)
            : delta_container_(allocator_type(allocator, "delta"))
        {}

        template < typename StateContainer > void update(StateContainer& state_container, const Node& node, T value)
        {
            // TODO: counter_g itself needs to be thread-safe
            // And operations from the same node needs to be synchronized.

            auto delta_it = delta_container_.find(node);
            if (delta_it != delta_container_.end())
            {
                delta_container_.update(delta_it, value);
            }
            else
            {
                auto state_it = state_container.find(node);
                if (state_it != state_container.end())
                {
                    delta_container_.update(node, state_it->second + value);
                }
                else
                {
                    delta_container_.update(node, value);
                }
            }
        }

    // private:
        counter_g< Node, T, Traits > delta_container_;
    };
}