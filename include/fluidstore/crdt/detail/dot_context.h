#pragma once

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/detail/dot_counters_base.h>

#include <fluidstore/btree/map.h>

namespace crdt
{
    template < typename CountersType, typename Dot, typename Tag > class dot_context
    {
        template < typename CountersTypeT, typename DotT, typename TagT > friend class dot_context;

        using dot_type = Dot;
        using replica_id_type = typename dot_type::replica_id_type;
        using counter_type = typename dot_type::counter_type;       

        struct default_context
        {
            template < typename T > void register_erase(const T&) {}
        };

        template< typename T, typename Allocator > auto get_allocator(Allocator& allocator)
        {
            return std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
        }

        static_assert(std::is_trivially_copyable_v< Dot >);
        static_assert(std::is_trivially_move_assignable_v< Dot >);
        static_assert(std::is_trivially_move_constructible_v< Dot >);

    public:
        // TODO: use pointer for value
        using counters_type = CountersType; 
        using size_type = typename CountersType::size_type;

        dot_context(counters_type& counters)
            : counters_(counters)
        {}
    
        dot_context(dot_context&&) = default;
        ~dot_context() = default;

        dot_context& operator = (dot_context&&) = default;

        dot_context(const dot_context&) = delete;
        dot_context& operator = (const dot_context&) = delete;

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, const dot_type& dot)
        {
            auto& counters = counters_.emplace(allocator, dot.replica_id, btree::set_base< counter_type >()).first->second;

            dot_counters_base< btree::set_base< counter_type >, Tag > values(counters);
            values.emplace(allocator, dot.counter);
        }

        template < typename Allocator, typename TagT, typename SizeTypeT > void insert(Allocator& allocator, const dot_context< CountersType, Dot, TagT >& dots)
        {
            for (auto& [replica_id, rcounters] : dots)
            {
                auto& lcounters = counters_.emplace(allocator, replica_id, btree::set_base< counter_type >()).first->second;

                dot_counters_base< btree::set_base< counter_type >, Tag > values(lcounters);
                values.insert(allocator, rcounters);
            }
        }

        counter_type get(const replica_id_type& replica_id) const
        {
            auto it = counters_.find(replica_id);
            if (it != counters_.end())
            {
                dot_counters_base< btree::set_base< counter_type >, Tag > values(it->second);
                return values.get();
            }

            return counter_type();
        }

        bool has(const dot_type& dot) const
        {
            auto it = counters_.find(dot.replica_id);
            if (it != counters_.end())
            {
                dot_counters_base< btree::set_base< counter_type >, Tag > values(it->second);
                return values.has(dot.counter);
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
            for (auto&& [replica_id, rcounters] : other)
            {
                auto& counters = counters_.emplace(allocator, replica_id, btree::set_base< counter_type >()).first->second;
                dot_counters_base< btree::set_base< counter_type >, Tag > values(counters);
                values.update(allocator, replica_id, rcounters, context);
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
            for (auto&& [replica_id, counters] : counters_)
            {
                dot_counters_base< btree::set_base< counter_type >, Tag > values(counters);
                values.collapse(allocator, replica_id, context);
            }
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            for (auto&& [replica_id, counters] : counters_)
            {
                counters.clear(allocator);
            }
            counters_.clear(allocator);
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
            for (const auto& [replica_id, counters]: counters_)
            {
                dot_counters_base< btree::set_base< counter_type >, Tag > values(counters);
                count += values.size();
            }

            return count;
        }

        auto begin() const { return counters_.begin(); }
        auto end() const { return counters_.end(); }
        auto empty() const { return counters_.empty(); }

    private:
        // TODO: constness
        counters_type& counters_;
    };
}