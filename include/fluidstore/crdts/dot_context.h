#pragma once

#include <fluidstore/crdts/dot.h>
#include <fluidstore/flat/set.h>

#include <set>

namespace crdt
{
    struct tag_delta {};
    struct tag_state {};

    template < typename ReplicaId, typename Counter, typename Allocator, typename Tag > class dot_context
    {
        template < typename ReplicaId, typename Counter, typename Allocator, typename Tag > friend class dot_context;

    public:
        using dot_type = dot< ReplicaId, Counter >;
        using allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< dot_type >;
        
        dot_context(Allocator allocator)
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

        void collapse()
        {
            default_context context;
            collapse(context);
        }

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

    template < typename ReplicaId, typename Counter, typename Tag > class dot_context2
    {
        using dot_type = dot< ReplicaId, Counter >;
        using size_type = typename flat::set< dot_type >::size_type;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template< typename T, typename Allocator > auto get_allocator(Allocator& allocator)
        {
            return std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);
        }

    public:
        dot_context2()
        {}

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, Args&&... args)
        {
            counters_.emplace(get_allocator< dot_type >(allocator), dot_type(std::forward< Args >(args)...));
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
            counters_.insert(get_allocator< dot_type >(allocator), other.counters_.begin(), other.counters_.end(), other.counters_.size());
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
            flat::vector< typename flat::set< dot_type >::iterator > iterators;
            auto iterators_alloc = get_allocator< typename flat::set< dot_type >::iterator >(allocator);

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
                            iterators.push_back(iterators_alloc, prev);
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
                counters_.erase(iterators_alloc, iterators[i - 1]);
            }

            iterators.clear(iterators_alloc);
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            counters_.clear(get_allocator< dot_type >(allocator));
        }

        auto begin() const { return counters_.begin(); }
        auto end() const { return counters_.end(); }
        auto size() const { return counters_.size(); }
        auto empty() const { return counters_.empty(); }
        void erase(const dot_type& dot) { counters_.erase(dot); }

    private:
        flat::set< dot_type > counters_;
    };
}