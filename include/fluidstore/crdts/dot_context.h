#pragma once

#include <fluidstore/crdts/dot.h>
#include <fluidstore/crdts/dot_counters_base.h>

#if defined(DOTCONTEXT_BTREE)
#include <fluidstore/btree/map.h>
#else
#include <fluidstore/flat/map.h>
#endif

namespace crdt
{
    template < typename Dot, typename Tag, typename SizeType = uint32_t > class dot_context
    {
        template < typename DotT, typename TagT, typename SizeTypeT > friend class dot_context;

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

        static_assert(std::is_trivially_copyable_v< Dot >);
        static_assert(std::is_trivially_move_assignable_v< Dot >);
        static_assert(std::is_trivially_move_constructible_v< Dot >);
                        
    public:
        dot_context() = default;
        dot_context(dot_context&&) = default;
        ~dot_context() = default;

        dot_context& operator = (dot_context&&) = default;

        dot_context(const dot_context&) = delete;
        dot_context& operator = (const dot_context&) = delete;

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
    #if defined(DOTCONTEXT_BTREE)
        mutable btree::map_base < replica_id_type, dot_counters_base< counter_type, Tag, size_type > > counters_;
    #else
        mutable flat::map_base< 
            replica_id_type, dot_counters_base< counter_type, Tag, size_type >, 
            flat::map_node< replica_id_type, dot_counters_base< counter_type, Tag, size_type > >,
            size_type 
        > counters_;
    #endif
    };
}