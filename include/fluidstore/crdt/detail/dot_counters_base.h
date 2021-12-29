#pragma once

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/tags.h>

#include <fluidstore/btree/set.h>

namespace crdt
{
    // TODO: the delta variant and state variant have different requirements for the types they keep:
    //  delta variant keeps sorted set
    //  state variant keeps just the latest. This is true after the merge, but not during the merge.

    template < typename CounterType, typename Tag, typename SizeType = uint32_t > class dot_counters_base
    {
        template < typename CounterTypeT, typename TagT, typename SizeTypeT > friend class dot_counters_base;

        using counter_type = CounterType;
        using size_type = SizeType;
        using tag_type = Tag;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };
            
    public:
        using counters_type = btree::set_base< counter_type >;

        dot_counters_base(counters_type& counters) :
            counters_(counters)
        {}

        dot_counters_base(dot_counters_base&& other) = default;
        ~dot_counters_base() = default;

        dot_counters_base< CounterType, Tag, SizeType >& operator = (dot_counters_base< CounterType, Tag, SizeType >&&) = default;

        dot_counters_base(const dot_counters_base&) = delete;
        dot_counters_base< CounterType, Tag, SizeType >& operator = (const dot_counters_base< CounterType, Tag, SizeType >&) = delete;

        counter_type get() const
        {
            return !counters_.empty() ? *--counters_.end() : counter_type();
        }

        bool has(counter_type counter) const
        {
            return counters_.find(counter) != counters_.end();
        }
        
        bool empty() const
        {
            return counters_.empty();
        }

        template < typename Allocator > void erase(Allocator& allocator, counter_type counter)
        {
            if (std::is_same_v< tag_type, tag_state >)
            {
                // In dot_kernel, we first add, than clear valueless dots. So in the end, counters might hold just one value, but during the merge, it is not like that.
                //assert(counters_.size() <= 1);
            }
            
            counters_.erase(allocator, counter);
        }

        template < typename Allocator > void emplace(Allocator& allocator, counter_type counter)
        {
            if (std::is_same_v< tag_type, tag_state >)
            {
                assert(counters_.empty());
            }
            else
            {
                // This happens during clear, we remember cleared counters
            }

            counters_.emplace(allocator, counter);
        }

        template < typename Allocator, typename Counters > void insert(Allocator& allocator, const Counters& counters)
        {
            if (std::is_same_v< tag_type, tag_state >)
            {
                assert(counters_.empty());
            }

            // assert(counters.counters_.size() == 1);  // delta variant can have more than 1 element

            counters_.insert(allocator, counters.begin(), counters.end());
        }

        size_type size() const
        {
            // TODO
            return (size_type)counters_.size();
        }

        template < typename Allocator, typename ReplicaId, typename Context > void collapse(Allocator& allocator, const ReplicaId& replica_id, Context& context)
        {
            auto next = counters_.begin();
            auto prev = next++;
            for (; next != counters_.end();)
            {
                if (*next == *prev + 1)
                {
                    // TODO: we should find a largest block to erase, not erase single element and continue
                    context.register_erase(dot< ReplicaId, counter_type >{ replica_id, *prev });
                    next = counters_.erase(allocator, prev);
                    prev = next++;
                }
                else
                {
                    break;
                }
            }
        }

        template < typename Allocator, typename ReplicaId, typename RCounters, typename Context >
        void update(Allocator& allocator, const ReplicaId& replica_id, RCounters& rcounters, Context& context)
        {
            if (std::is_same_v< tag_type, tag_state >)
            {
                // TODO: investigate
                // assert(counters_.size() <= 1);      // state variant should have 0 or 1 elements
            }
                        
            // TODO: this assert should really by true. The issue is in map::insert with value_mv - it is not delta there.
            //static_assert(std::is_same_v< RCounters::tag_type, tag_delta >);
            // 
            // assert(rcounters.size() == 1);   // delta variant can have N elements, and merging two state variants does not make sense

            if (counters_.size() == 0)
            {
                // Trivial append
                counters_.insert(allocator, rcounters.begin(), rcounters.end());
            }
            else if (counters_.size() == 1 && rcounters.size() == 1)
            {
                // Maybe in-place replace
                if (*counters_.begin() + 1 == *rcounters.begin())
                {
                    auto counter = *counters_.begin();

                    // TODO: in-place update
                    counters_.insert(allocator, *counters_.begin() + 1);
                    counters_.erase(allocator, counters_.begin());
                
                    // No need to collapse here, but have to notify upper layer about removal
                    context.register_erase(dot< ReplicaId, counter_type >{ replica_id, counter });

                    return;
                }
                else
                {
                    counters_.insert(allocator, rcounters.begin(), rcounters.end());
                }
            }
            else
            {
                // TODO: two sets merge
                counters_.insert(allocator, rcounters.begin(), rcounters.end());
            }

            if (std::is_same_v< Tag, tag_state >)
            {
                collapse(allocator, replica_id, context);
            }
        }

        template < typename Allocator, typename ReplicaId, typename Counters >
        void merge(Allocator& allocator, const ReplicaId& replica_id, Counters& counters)
        {
            default_context context;
            update(allocator, replica_id, counters, context);
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            counters_.clear(allocator);
        }
            
        counters_type& counters_;
    };
}
