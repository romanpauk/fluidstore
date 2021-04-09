#pragma once

#include <fluidstore/crdts/dot.h>
#include <fluidstore/flat/set.h>

namespace crdt
{
    struct tag_delta {};
    struct tag_state {};

    template < typename Dot, typename Tag > class dot_context
    {
        template < typename Dot, typename Tag > friend class dot_context;

        using dot_type = Dot;
        using replica_id_type = typename dot_type::replica_id_type;
        using counter_type = typename dot_type::replica_id_type;

        using size_type = typename flat::set_base< dot_type >::size_type;

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template< typename T, typename Allocator > auto get_allocator(Allocator& allocator)
        {
            return std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
        }

    public:
        dot_context()
        {}

        dot_context(dot_context&& other)
            : counters_(std::move(other.counters_))
        {}

        ~dot_context() = default;

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, Args&&... args)
        {
            counters_.emplace(allocator, dot_type{ std::forward< Args >(args)... });
        }

        template < typename Allocator, typename It > void insert(Allocator& allocator, It begin, It end)
        {
            counters_.insert(allocator, begin, end);
        }

        counter_type get(const replica_id_type& replica_id) const
        {
            auto counter = counter_type();
            auto it = counters_.upper_bound(dot_type{ replica_id, 0 });
            while (it != counters_.end() && it->replica_id == replica_id)
            {
                counter = it++->counter;
            }

            return counter;
        }

        auto find(const dot_type& dot) const
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
                            next = counters_.erase(allocator, prev);
                            prev = next++;
                        }
                        else
                        {
                            next = counters_.upper_bound({ next->replica_id, std::numeric_limits< counter_type >::max() });
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