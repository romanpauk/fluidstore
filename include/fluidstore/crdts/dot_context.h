#pragma once

#include <fluidstore/crdts/dot.h>
#include <fluidstore/crdts/allocator_traits.h>

#include <fluidstore/flat/set.h>

#include <set>

namespace crdt
{
    struct tag_delta {};
    struct tag_state {};

    template < typename ReplicaId, typename Counter, typename Tag > class dot_context
    {
        template < typename ReplicaId, typename Counter, typename Tag > friend class dot_context;

        using dot_type = dot< ReplicaId, Counter >;
        using size_type = typename flat::set_base< dot_type >::size_type;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template< typename T, typename Allocator > auto get_allocator(Allocator& allocator)
        {
            return std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
        }

        template < typename ReplicaId, typename Counter, typename Tag > friend class dot_context2;

    public:
        dot_context()
        {}

        dot_context(dot_context&& other)
            : counters_(std::move(other.counters_))
        {}

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, Args&&... args)
        {
            counters_.emplace(allocator, dot_type{ std::forward< Args >(args)... });
        }

        template < typename Allocator, typename It > void insert(Allocator& allocator, It begin, It end)
        {
            counters_.insert(allocator, begin, end);
        }

        Counter get(const ReplicaId& replica_id) const
        {
            auto counter = Counter();
            auto it = counters_.upper_bound(dot< ReplicaId, Counter >{ replica_id, 0 });
            while (it != counters_.end() && it->replica_id == replica_id)
            {
                counter = it++->counter;
            }

            return counter;
        }

        auto find(const dot< ReplicaId, Counter >& dot) const
        {
            return counters_.find(dot);
        }

        template < typename Allocator, typename DotContextT > void merge(Allocator& allocator, const DotContextT& other)
        {
            default_context context;
            merge(allocator, other, context);
        }

        template < typename Allocator, typename DotContextT, typename Context > void merge(Allocator& allocator, const DotContextT& other, Context& context)
        {
            counters_.insert(get_allocator< dot_type >(allocator), other.counters_.begin(), other.counters_.end());
            if (std::is_same_v< Tag, tag_state >)
            {
                collapse(allocator, context);
            }
        }

        const auto& get() const { return counters_; }

        template < typename Allocator > void collapse(Allocator& allocator)
        {
            default_context context;
            collapse(allocator, context);
        }

        template < typename Allocator, typename Context > void collapse(Allocator& allocator, Context& context)
        {
            auto tmp = allocator_traits< Allocator >::get_allocator< crdt::tag_delta >(allocator);
            flat::vector_base< typename flat::set_base< dot_type >::iterator > iterators;
            
            if (counters_.size() > 1)
            {
                auto next = counters_.begin();
                auto prev = next++;
                for (; next != counters_.end();)
                {
                    if (next->replica_id == prev->replica_id)
                    {
                        if (next->counter == prev->counter + 1)
                        {
                            context.register_erase(*prev);
                            iterators.push_back(tmp, prev);
                            ++prev;
                            ++next;
                        }
                        else
                        {
                            next = counters_.upper_bound({ next->replica_id, std::numeric_limits< Counter >::max() });
                            if (next != counters_.end())
                            {
                                prev = next++;
                            }
                        }
                    }
                    else
                    {
                        prev = next;
                        ++next;
                    }
                }
            }

            for (size_type i = iterators.size(); i > 0; --i)
            {
                counters_.erase(allocator, iterators[i - 1]);
            }

            iterators.clear(tmp);
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            counters_.clear(get_allocator< dot_type >(allocator));
        }

        template < typename Allocator > void erase(Allocator& allocator, const dot_type& dot) { counters_.erase(allocator, dot); }

        auto begin() const { return counters_.begin(); }
        auto end() const { return counters_.end(); }
        auto size() const { return counters_.size(); }
        auto empty() const { return counters_.empty(); }
       
    private:
        flat::set_base< dot_type > counters_;
    };
}