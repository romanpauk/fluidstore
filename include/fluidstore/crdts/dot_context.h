#pragma once

#include <fluidstore/crdts/dot.h>
#include <fluidstore/flat/map.h>

namespace crdt
{
    struct tag_delta {};
    struct tag_state {};

    template < typename CounterType, typename Tag, typename SizeType = uint32_t > class dot_counters_base
    {
        template < typename CounterType, typename Tag, typename SizeType > friend class dot_counters_base;

        using counter_type = CounterType;
        using size_type = SizeType;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

    public:
        dot_counters_base() = default;
        dot_counters_base(dot_counters_base&& other) = default;
        ~dot_counters_base() = default;

        counter_type get() const
        {
            return !counters_.empty() ? counters_.back() : counter_type();
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
            counters_.erase(allocator, counter);
        }

        template < typename Allocator > void emplace(Allocator& allocator, counter_type counter)
        {
            counters_.emplace(allocator, counter);
        }

        template < typename Allocator, typename Counters > void insert(Allocator& allocator, const Counters& counters)
        {
            counters_.insert(allocator, counters.counters_);
        }

        size_type size() const
        {
            return counters_.size();
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
            if (counters_.size() == 0)
            {
                // Trivial append
                counters_.insert(allocator, rcounters.counters_);
            }
            else if (counters_.size() == 1 && rcounters.size() == 1)
            {
                // Maybe in-place replace
                if (*counters_.begin() == *rcounters.counters_.begin() + 1)
                {
                    counters_.update(counters_.begin(), *counters_.begin() + 1);

                    // No need to collapse here
                    return;
                }
                else
                {
                    counters_.insert(allocator, rcounters.counters_);
                }
            }
            else
            {
                // TODO: two sets merge
                counters_.insert(allocator, rcounters.counters_);
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

        flat::set_base< counter_type, size_type > counters_;
    };

    template < typename Dot, typename Tag, typename SizeType = uint32_t > class dot_context
    {
        template < typename Dot, typename Tag, typename SizeType > friend class dot_context;

        using dot_type = Dot;
        using replica_id_type = typename dot_type::replica_id_type;
        using counter_type = typename dot_type::counter_type;

        using size_type = SizeType;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template< typename T, typename Allocator > auto get_allocator(Allocator& allocator)
        {
            return std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
        }

    public:
        dot_context() = default;
        dot_context(dot_context&& other) = default;
        ~dot_context() = default;

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, const dot_type& dot)
        {
            auto pairb = counters_.emplace(allocator, dot.replica_id, dot_counters_base< counter_type, Tag, size_type >());
            pairb.first->second.emplace(allocator, dot.counter);
        }

        template < typename Allocator, typename TagT, typename SizeTypeT > void insert(Allocator& allocator, const dot_context< Dot, TagT, SizeTypeT >& dots)
        {
            for (auto& [replica_id, counters] : dots)
            {
                auto it = counters_.emplace(allocator, replica_id, dot_counters_base< counter_type, Tag, size_type >());
                it.first->second.insert(allocator, counters);
            }
        }

        counter_type get(const replica_id_type& replica_id) const
        {
            auto it = counters_.find(replica_id);
            if (it != counters_.end())
            {
                return it->second.get();
            }

            return counter_type();
        }

        bool has(const dot_type& dot) const
        {
            auto it = counters_.find(dot.replica_id);
            if (it != counters_.end())
            {
                return it->second.has(dot.counter);
            }

            return false;
        }

        template < typename Allocator, typename DotContextT > void merge(Allocator& allocator, const DotContextT& other)
        {
            default_context context;
            merge(allocator, other, context);
        }

        template < typename Allocator, typename DotContextT, typename Context > void merge(Allocator& allocator, const DotContextT& other, Context& context)
        {
            for (auto& [replica_id, rcounters] : other.counters_)
            {
                auto& counters = counters_.emplace(allocator, replica_id, dot_counters_base< counter_type, Tag, size_type >()).first->second;
                counters.update(allocator, replica_id, rcounters, context);
            }
        }

        auto& get_dots(const replica_id_type& replica_id) const 
        {
            auto it = counters_.find(replica_id);
            assert(it != counters_.end());
            return it->second.counters_;
        }

        template < typename Allocator > void collapse(Allocator& allocator)
        {
            default_context context;
            collapse(allocator, context);
        }

        template < typename Allocator, typename Context > void collapse(Allocator& allocator, Context& context)
        {
            for (auto& [replica_id, counters] : counters_)
            {
                counters.collapse(allocator, replica_id, context);
            }
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            for (auto& [replica_id, counters] : counters_)
            {
                counters.clear(allocator);
            }
            counters_.clear(get_allocator< dot_type >(allocator));
        }

        template < typename Allocator > void erase(Allocator& allocator, const dot_type& dot) 
        { 
            auto it = counters_.find(dot.replica_id);
            if (it != counters_.end())
            {
                it->second.erase(allocator, dot.counter);
                if (it->second.empty())
                {
                    counters_.erase(allocator, it);
                }
            } 
        }

        size_type size() const
        {
            // TODO: this seems to be used only for testing, as well as find().
            size_type count = 0;
            for (auto& [replica_id, counters]: counters_)
            {
                count += counters.size();
            }

            return count;
        }

        auto begin() const { return counters_.begin(); }
        auto end() const { return counters_.end(); }
        auto empty() const { return counters_.empty(); }

    private:
        // TODO: constness
        mutable flat::map_base< 
            replica_id_type, dot_counters_base< counter_type, Tag, size_type >, 
            flat::map_node< replica_id_type, dot_counters_base< counter_type, Tag, size_type > >,
            size_type 
        > counters_;
    };
}