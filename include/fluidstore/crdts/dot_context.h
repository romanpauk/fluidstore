#pragma once
#include <fluidstore/crdts/dot.h>

#include <set>

namespace crdt
{
    template < typename ReplicaId, typename Counter, typename Allocator, typename Tag > class dot_context
    {
        template < typename ReplicaId, typename Counter, typename Allocator, typename Tag > friend class dot_context;

    public:
        using allocator_type = Allocator;
        using dot_type = dot< ReplicaId, Counter >;

        dot_context(allocator_type allocator)
            : counters_(allocator)
        {}

        template < typename... Args > void emplace(Args&&... args)
        {
            counters_.emplace(std::forward< Args >(args)...);
        }

        template < typename It > void insert(It begin, It end)
        {
            counters_.insert(begin, end);
        }

        auto find(const dot< ReplicaId, Counter >& dot) const
        {
            return counters_.find(dot);
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

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template < typename DotContextT > void merge(const DotContextT& other)
        {
            default_context context;
            merge(other, context);
        }

        template < typename DotContextT, typename Context > void merge(const DotContextT& other, Context& context)
        {
            insert(other.counters_.begin(), other.counters_.end());
            if (std::is_same_v< Tag, tag_state >)
            {
                collapse(context);
            }
        }

        const auto& get() const { return counters_; }

        template < typename Context > void collapse(Context& context)
        {
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
                            prev = counters_.erase(prev);
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
        }

        auto begin() const { return counters_.begin(); }
        auto end() const { return counters_.end(); }
        auto size() const { return counters_.size(); }
        auto empty() const { return counters_.empty(); }
        void erase(const dot_type& dot) { counters_.erase(dot); }

    private:
        std::set< dot_type, std::less< dot_type >, allocator_type > counters_;
    };
}