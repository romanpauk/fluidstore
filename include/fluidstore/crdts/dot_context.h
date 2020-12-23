#pragma once

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

        template < typename Dot > void emplace(Dot&& dot)
        {
            counters_.emplace(std::forward< Dot >(dot));
        }

        template < typename It > void insert(It begin, It end)
        {
            counters_.insert(begin, end);
        }

        bool find(const dot< ReplicaId, Counter >& dot) const
        {
            return counters_.find(dot) != counters_.end();
        }

        void remove(const dot< ReplicaId, Counter >& dot)
        {
            counters_.erase(dot);
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
                auto it = counters_.begin();
                auto d = *it++;
                for (; it != counters_.end();)
                {
                    if (it->replica_id == d.replica_id)
                    {
                        if (it->counter == d.counter + 1)
                        {
                            it = counters_.erase(it);
                        }
                        else
                        {
                            it = counters_.upper_bound({ d.replica_id, std::numeric_limits< Counter >::max() });
                            if (it != counters_.end())
                            {
                                d = *it;
                                ++it;
                            }
                        }
                    }
                    else
                    {
                        d = *it;
                        ++it;
                    }
                }
            }
        }

    private:
        std::set< dot< ReplicaId, Counter >, std::less< dot< ReplicaId, Counter > >, allocator_type > counters_;
    };
}