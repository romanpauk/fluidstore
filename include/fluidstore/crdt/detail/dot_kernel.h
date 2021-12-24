#pragma once

// Merge algorithm is based on the article "An Optimized Conflict-free Replicated Set"
// https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf

#include <fluidstore/crdt/detail/dot_context.h>
#include <fluidstore/crdt/detail/dot_kernel_allocator.h>
#include <fluidstore/crdt/detail/dot_kernel_iterator.h>
#include <fluidstore/crdt/detail/dot_kernel_value.h>
#include <fluidstore/crdt/detail/metadata.h>

#include <fluidstore/crdt/allocator.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <fluidstore/flat/map.h>

#include <fluidstore/btree/map.h>
#include <fluidstore/btree/set.h>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Container, typename Tag, 
        typename Metadata = detail::metadata< Key, Tag, Allocator, detail::metadata_local > > 
    class dot_kernel
    {
        template < typename KeyT, typename ValueT, typename AllocatorT, typename ContainerT, typename TagT, typename Metadata > friend class dot_kernel;

    public:
        using allocator_type = Allocator;           
        using metadata_type = Metadata;

        using replica_type = typename allocator_type::replica_type;
        using replica_id_type = typename replica_type::replica_id_type;
        using counter_type = typename replica_type::counter_type;

        using dot_type = dot< replica_id_type, counter_type >;
        using dot_kernel_type = dot_kernel< Key, Value, allocator_type, Container, Tag, Metadata >;
        using dot_context_type = dot_context< dot_type, Tag >;
        
        using dot_kernel_value_type = dot_kernel_value< Key, Value, Allocator, dot_context_type, dot_kernel_type >;

    #if defined(DOTKERNEL_BTREE)
        // TODO: use pointer for value
        using values_type = btree::map_base< Key, dot_kernel_value_type >;
    #else
        using values_type = flat::map_base< Key, Value, dot_kernel_value_type >;
    #endif

        using iterator = dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type >;
        using const_iterator = dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type >;
               
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

        template < typename AllocatorT, typename MetadataT > struct value_context
        {
            value_context(AllocatorT& allocator, MetadataT& metadata)
                : allocator_(allocator)
                , metadata_(metadata)
            {}

            void register_erase(const dot_type& dot) 
            {                
                auto replica = metadata_.get_replica_data(dot.replica_id);
                if (replica)
                {
                    replica->dots.erase(allocator_, dot.counter);
                }
            }

        private:
            AllocatorT& allocator_;
            MetadataT& metadata_;
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

            metadata_.clear(allocator);            
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

            // TODO: this is added to make replica_data smaller, but it has a price as we need to search for it twice.
            // btree::map< replica_id_type, btree::set_base< counter_type >, std::less< replica_id_type >, decltype(tmp) > rvisited(tmp);         

            // Merge values
            for (const auto& [rkey, rvalue] : other.get_values())
            {
            #if defined(DOTKERNEL_BTREE)
                auto lpb = values_.emplace(allocator, rkey, dot_kernel_value_type(allocator, rkey, this));
            #else
                auto lpb = values_.emplace(allocator, allocator, rkey, this);
            #endif
                auto& lvalue = *lpb.first;

                value_context value_ctx(allocator, metadata_);
            #if defined(DOTKERNEL_BTREE)
                lvalue.second.merge(allocator, rvalue, value_ctx);
            #else
                lvalue.merge(allocator, rvalue, value_ctx);
            #endif

                for (const auto& [replica_id, rdots] : rvalue.dots)
                {
                    // Track visited dots
                                        
                    // TODO: better insert
                    //rvisited[replica_id].insert(tmp, rdots.counters_.begin(), rdots.counters_.end());

                    auto& ldata = metadata_.get_replica_data(allocator, replica_id); // replica_.emplace(allocator, replica_id, replica_data()).first->second;
                    ldata.visited.insert(tmp, rdots.counters_.begin(), rdots.counters_.end());

                    for (const auto& counter : rdots.counters_)
                    {
                        // Create dot -> key link
                        ldata.dots.emplace(allocator, counter, rkey);
                    }
                }

                // Support for insert / emplace pairb result
                ctx.register_insert(lpb);
            }

            // Merge replicas
            for (auto& [replica_id, rdata] : other.get_replica_map())
            {
                // Merge global counters
                // auto& ldata = replica_.emplace(allocator, replica_id, replica_data()).first->second;
                auto& ldata = metadata_.get_replica_data(allocator, replica_id);
                ldata.counters.merge(allocator, replica_id, rdata.counters);

                const auto& rdata_counters = rdata.counters.counters_;

                // Determine deleted values (those are the ones we have not visited in a loop over values).

                // TODO: deque
                flat::vector < counter_type, decltype(tmp) > rdotsvalueless(tmp);

                //*                
                std::set_difference(
                    rdata_counters.begin(), rdata_counters.end(),
                    ldata.visited.begin(), ldata.visited.end(),
                    std::back_inserter(rdotsvalueless)
                );
                
                ldata.visited.clear(tmp);
                //*/

                /*
                auto it = rvisited.find(replica_id);
                if (it != rvisited.end())
                {
                    auto& rdata_visited = (*it).second;

                    std::set_difference(
                        rdata_counters.begin(), rdata_counters.end(),
                        rdata_visited.begin(), rdata_visited.end(),
                        std::back_inserter(rdotsvalueless)
                    );

                    rdata_visited.clear(tmp);
                }
                else
                {
                    // TODO:
                    btree::set_base< counter_type > empty_visited;
                    std::set_difference(
                        rdata_counters.begin(), rdata_counters.end(),
                        // ldata.visited.begin(), ldata.visited.end(),
                        empty_visited.begin(), empty_visited.end(),
                        std::back_inserter(rdotsvalueless)
                    );

                    // TODO:
                    // rdotsavalueless.assign(rdata_counters.begin(), rdata_counters.end());
                }
                */

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
                metadata_.get_replica_data(allocator, replica_id).counters.insert(allocator, counters);
            }
        }

        void add_counter_dot(const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            metadata_.get_replica_data(allocator, dot.replica_id).counters.emplace(allocator, dot.counter);
        }

        // TODO: const
        dot_type get_next_dot()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto replica_id = allocator.get_replica().get_id();
            counter_type counter = 1;
            
            auto replica = metadata_.get_replica_data(replica_id);
            if (replica)
            {
                counter = replica->counters.get() + 1;
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
                
        const auto& get_replica_map() const { return metadata_.get_replica_map(); }

        const values_type& get_values() const { return values_; }

    private:
        values_type values_;
        metadata_type metadata_;
    };
}