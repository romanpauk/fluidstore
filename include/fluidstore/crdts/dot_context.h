#pragma once
#include <fluidstore/crdts/dot.h>

#include <set>

namespace crdt
{
    template < typename ReplicaId, typename Counter, typename Allocator > class dot_context
    {
        template < typename ReplicaId, typename Counter, typename Allocator > friend class dot_context;

    public:
        typedef Allocator allocator_type;

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

        template < typename AllocatorT > void merge(const dot_context< ReplicaId, Counter, AllocatorT >& other)
        {
            insert(other.counters_.begin(), other.counters_.end());
            collapse();
        }

        const auto& get() const { return counters_; }

        void collapse()
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

    private:
        std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > counters_;
    };
}