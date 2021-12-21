#pragma once

// Merge algorithm is based on the article "An Optimized Conflict-free Replicated Set"
// https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf

#include <fluidstore/crdt/detail/dot_context.h>
#include <fluidstore/crdt/detail/dot_kernel_allocator.h>
#include <fluidstore/crdt/detail/dot_kernel_iterator.h>
#include <fluidstore/crdt/detail/dot_kernel_value.h>

#include <fluidstore/crdt/allocator.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <fluidstore/flat/map.h>

#include <fluidstore/btree/map.h>
#include <fluidstore/btree/set.h>

namespace crdt
{
    /*
        // This will not save any search.

    template < typename Key, typename Counter, typename Tag > struct dot_kernel_replica_data;
    
    template < typename Key, typename Counter > struct dot_kernel_replica_data < Key, Counter, tag_state >
    {
        dot_counters_base< Counter, tag_state > counters;

    #if defined(DOTKERNEL_BTREE)
        btree::map_base< Counter, Key > dots;
    #else
        flat::map_base< Counter, Key > dots;
    #endif       
    };

    template < typename Key, typename Counter > struct dot_kernel_replica_data < Key, Counter, tag_delta >
        : dot_kernel_replica_data< Key, Counter, tag_state >
    {
    #if defined(DOTCOUNTERS_BTREE)
        // Temporary merge data
        btree::set_base< Counter > visited;
    #else        
        // Temporary merge data
        flat::set_base< Counter > visited;
    #endif
    };
    */

    template < typename Key, typename Value, typename Allocator, typename Container, typename Tag > class dot_kernel
    {
        template < typename KeyT, typename ValueT, typename AllocatorT, typename ContainerT, typename TagT > friend class dot_kernel;

    public:
        using allocator_type = Allocator;           
    
        using replica_type = typename allocator_type::replica_type;
        using replica_id_type = typename replica_type::replica_id_type;
        using counter_type = typename replica_type::counter_type;

        using dot_type = dot< replica_id_type, counter_type >;
        using dot_kernel_type = dot_kernel< Key, Value, allocator_type, Container, Tag >;
        using dot_context_type = dot_context< dot_type, Tag >;
        
        using dot_kernel_value_type = dot_kernel_value< Key, Value, Allocator, dot_context_type, dot_kernel_type >;

    #if defined(DOTKERNEL_BTREE)
        using values_type = btree::map_base< Key, dot_kernel_value_type >;
    #else
        using values_type = flat::map_base< Key, Value, dot_kernel_value_type >;
    #endif

        using iterator = dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type >;
        using const_iterator = dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type >;

        // TODO: parametrize by tag, tag_state does not have to hold temporary merge data.
        struct replica_data
        {
            // Persistent data
            dot_counters_base< counter_type, Tag > counters;

        #if defined(DOTKERNEL_BTREE)
            btree::map_base< counter_type, Key > dots;
        #else
            flat::map_base< counter_type, Key > dots;            
        #endif       
        #if defined(DOTCOUNTERS_BTREE)
            // Temporary merge data
            btree::set_base< counter_type > visited;
        #else
            // Temporary merge data
            flat::set_base< counter_type > visited;
        #endif
        };
        
    #if defined(DOTKERNEL_BTREE)
        using replicas_type = btree::map_base< replica_id_type, replica_data >;
    #else
        using replicas_type = flat::map_base< replica_id_type, replica_data >;
    #endif

        struct context
        {
            void register_insert(std::pair< typename values_type::iterator, bool >) {}
            void register_erase(typename values_type::iterator) {}
        };

        struct insert_context: public context
        {
            void register_insert(std::pair< typename values_type::iterator, bool > pb) { result = pb; }
            std::pair< typename values_type::iterator, bool > result;
        };

        struct erase_context: public context
        {
            void register_erase(typename values_type::iterator it) { iterator = it; ++count; }
            typename values_type::iterator iterator;
            size_t count = 0;
        };

        template < typename AllocatorT, typename ReplicaMap > struct value_context
        {
            value_context(AllocatorT& allocator, ReplicaMap& replica)
                : allocator_(allocator)
                , replica_(replica)
            {}

            void register_erase(const dot_type& dot) 
            { 
                auto it = replica_.find(dot.replica_id);
                if (it != replica_.end())
                {
                    it->second.dots.erase(allocator_, dot.counter);
                }
            }

        private:
            Allocator& allocator_;
            ReplicaMap& replica_;
        };
           
        dot_kernel() = default;
        dot_kernel(dot_kernel_type&& other) = default;             
        dot_kernel_type& operator = (dot_kernel_type&& other) = default;
        
        ~dot_kernel()
        {
            reset();
        }
    
        void reset()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();

            for (auto& value : values_)
            {
                value.second.dots.clear(allocator);
            }
            values_.clear(allocator);

            for (auto& [replica_id, data] : replica_)
            {
                data.counters.clear(allocator);
                data.dots.clear(allocator);
            }
            replica_.clear(allocator);
        }

        template < typename DotKernel >
        void merge(const DotKernel& other)
        {
            context ctx;
            merge(other, ctx);
        }

        template < typename DotKernel, typename Context >
        void merge(const DotKernel& other, Context& ctx)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > tmp(allocator.get_replica(), arena);

            //btree::map< replica_id_type, btree::set_base< counter_type >, std::less< replica_id_type >, decltype(tmp) > rvisited(tmp);

            for (const auto& [replica_id, rdata] : other.get_replica())
            {
                // Merge counters
                auto& ldata = replica_.emplace(allocator, replica_id, replica_data()).first->second;
                ldata.counters.merge(allocator, replica_id, rdata.counters);
            }

            // Merge values
            for (const auto& [rkey, rvalue] : other.get_values())
            {
            #if defined(DOTKERNEL_BTREE)
                auto lpb = values_.emplace(allocator, rkey, dot_kernel_value_type(allocator, rkey, this));
            #else
                auto lpb = values_.emplace(allocator, allocator, rkey, this);
            #endif
                auto& lvalue = *lpb.first;

                value_context value_ctx(allocator, replica_);
            #if defined(DOTKERNEL_BTREE)
                lvalue.second.merge(allocator, rvalue, value_ctx);
            #else
                lvalue.merge(allocator, rvalue, value_ctx);
            #endif

                for (const auto& [replica_id, rdots] : rvalue.dots)
                {
                    auto& ldata = replica_.emplace(allocator, replica_id, replica_data()).first->second;
                    
                    // Track visited dots
                    ldata.visited.insert(tmp, rdots.counters_.begin(), rdots.counters_.end());     

                    // rvisited[replica_id].insert(tmp, rdots.counters_.begin(), rdots.counters_.end());

                    for (const auto& counter : rdots.counters_)
                    {
                        // Create dot -> key link
                        ldata.dots.emplace(allocator, counter, rkey);
                    }
                }

                // Support for insert / emplace pairb result
                ctx.register_insert(lpb);
            }

            for (auto& [replica_id, rdata] : other.get_replica())
            {
                auto replica_it = replica_.find(replica_id);
                if (replica_it != replica_.end())
                {
                    auto& ldata = replica_it->second;
                    const auto& rdata_counters = rdata.counters.counters_;
                    // const auto& rdata_visited = rvisited[replica_id];

                    // Find dots that were not visited during processing of values (thus valueless). Those are the ones to be removed.
                    // TODO: this should be deque.
                    flat::vector < counter_type, decltype(tmp) > rdotsvalueless(tmp);
                    std::set_difference(
                        rdata_counters.begin(), rdata_counters.end(),
                        ldata.visited.begin(), ldata.visited.end(),
                        std::back_inserter(rdotsvalueless)
                    );

                    // TODO: other counters and visited can be tracked on rdata.
                    // Only after there will be valueless dots, we will query ldata and clear it.
                    // This way persistent replica_data will be two members less.

                    if (!rdotsvalueless.empty())
                    {
                        for (const auto& counter : rdotsvalueless)
                        {
                            auto counter_it = ldata.dots.find(counter);
                            if (counter_it != ldata.dots.end())
                            {
                                auto& lkey = counter_it->second;
                                auto values_it = values_.find(lkey);
                                values_it->second.dots.erase(allocator, dot_type{ replica_id, counter });
                            
                                if (values_it->second.dots.empty())
                                {
                                    auto it = values_.erase(allocator, values_it);

                                    // Support for erase iterator return value
                                    ctx.register_erase(it);
                                }

                                ldata.dots.erase(allocator, counter_it);
                            }
                        }
                    }

                    ldata.visited.clear(tmp);
                }
            }
        }

        // TODO: can we get rid of this callback?
        void update(const Key& key)
        {
            static_cast<Container*>(this)->update(key);
        }

        const_iterator begin() const { return values_.begin(); }
        iterator begin() { return values_.begin(); }
        const_iterator end() const { return values_.end(); }
        iterator end() { return values_.end(); }
        const_iterator find(const Key& key) const { return values_.find(key); }
        iterator find(const Key& key) { return values_.find(key); }

        template < typename Delta > void clear(Delta& delta)
        {
            for (const auto& [value, data] : values_)
            {
                delta.add_counter_dots(data.dots);
            }
        }

        bool empty() const
        {
            return values_.empty();
        }
        
        size_t size() const
        {
            return values_.size();
        }       
    
        template < typename Dots > void add_counter_dots(const Dots& dots)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            for (auto& [replica_id, counters] : dots)
            {
                replica_.emplace(allocator, replica_id, replica_data()).first->second.counters.insert(allocator, counters);
            }
        }

        void add_counter_dot(const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            replica_.emplace(allocator, dot.replica_id, replica_data()).first->second.counters.emplace(allocator, dot.counter);
        }

        // TODO: const
        dot_type get_next_dot()
        {
            auto replica_id = static_cast<Container*>(this)->get_allocator().get_replica().get_id();
            counter_type counter = 1;
            auto it = replica_.find(replica_id);
            if (it != replica_.end())
            {
                counter = it->second.counters.get() + 1;
            }
            
            return { replica_id, counter };
        }

        void add_value(const Key& key, const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
        #if defined(DOTKERNEL_BTREE)
            auto& data = *values_.emplace(allocator, key, dot_kernel_value_type(allocator, key, nullptr)).first;
        #else
            auto& data = *values_.emplace(allocator, allocator, key, nullptr).first;
        #endif
            data.second.dots.emplace(allocator, dot);
        }

        template < typename ValueT > void add_value(const Key& key, const dot_type& dot, ValueT&& value)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            
        #if defined(DOTKERNEL_BTREE)
            auto& data = *values_.emplace(allocator, key, dot_kernel_value_type(allocator, key, nullptr)).first;
            data.second.dots.emplace(allocator, dot);
        #else
            auto& data = *values_.emplace(allocator, allocator, key, nullptr).first;
            data.second.dots.emplace(allocator, dot);
        #endif
            data.second.value.merge(value);
        }

        const replicas_type& get_replica() const { return replica_; }
        const values_type& get_values() const { return values_; }

    private:
        values_type values_;
        replicas_type replica_;
    };
}
