#pragma once

// Merge algorithm is based on the article "An Optimized Conflict-free Replicated Set"
// https://pages.lip6.fr/Marek.Zawirski/papers/RR-8083.pdf

#include <fluidstore/crdt/tags.h>
#include <fluidstore/crdt/detail/dot_kernel_allocator.h>
#include <fluidstore/crdt/detail/dot_kernel_iterator.h>
#include <fluidstore/crdt/detail/dot_kernel_value.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_btree.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_stl.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_flat.h>

#include <fluidstore/crdt/allocator.h>
#include <fluidstore/memory/buffer_allocator.h>

#include <deque>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Container, typename Tag, 
        typename MetadataTag = detail::metadata_tag_btree_local,
        typename Metadata = detail::dot_kernel_metadata< Key, Tag, Allocator, MetadataTag > > 
    class dot_kernel
        : public detail::dot_kernel_metadata_base< Container, Metadata, MetadataTag >
    {
        template < typename KeyT, typename ValueT, typename AllocatorT, typename ContainerT, typename TagT, typename MetadataTagT, typename MetadataT > friend class dot_kernel;

    public:
        using allocator_type = Allocator;
        using metadata_type = Metadata;

        using replica_type = typename allocator_type::replica_type;
        using replica_id_type = typename replica_type::replica_id_type;
        using counter_type = typename replica_type::counter_type;

        using dot_type = dot< replica_id_type, counter_type >;
        using dot_kernel_type = dot_kernel< Key, Value, allocator_type, Container, Tag, MetadataTag, Metadata >;

        using dot_kernel_value_type = dot_kernel_value < Key, Value, allocator_type, typename Metadata::value_type_dots_type, dot_kernel_type >;

        using values_type = typename Metadata::template values_map_type< Key, dot_kernel_value_type >;

        using iterator = dot_kernel_iterator< typename values_type::iterator, Key, typename dot_kernel_value_type::value_type >;
        using const_iterator = dot_kernel_iterator< typename values_type::const_iterator, Key, typename dot_kernel_value_type::value_type >;

        struct context
        {
            void register_insert(std::pair< typename values_type::iterator, bool >) {}
            void register_erase(typename values_type::iterator) {}
        };

        struct insert_context : public context
        {
            void register_insert(std::pair< typename values_type::iterator, bool > pb) { result = pb; }
            std::pair< typename values_type::iterator, bool > result;
        };

        struct erase_context : public context
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
                metadata_.replica_dots_erase(allocator_, dot.replica_id, dot.counter);
            }

        private:
            AllocatorT& allocator_;
            MetadataT& metadata_;
        };

        struct counters_context
        {
            void register_erase(const dot_type&) {}
        };

        // TODO: values_ belong to metadata.
        dot_kernel()            
            : values_(
                typename std::allocator_traits< Allocator >::template rebind_alloc< typename values_type::allocator_type::value_type >(
                    static_cast<Container*>(this)->get_allocator()
                )
            )
        {}

        dot_kernel(dot_kernel_type&& other) = default;             
        dot_kernel_type& operator = (dot_kernel_type&& other) = default;
        
        ~dot_kernel()
        {
            reset();
        }
    
        void reset()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto& meta = this->get_metadata();
            meta.values_clear(allocator, values_);
            meta.clear(allocator);            
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

            memory::static_buffer< temporary_buffer_size > buffer;
            memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
            crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);
                       
            typename Metadata::template visited_map_type< decltype(tmp) > visited(tmp);
        
            auto& meta = this->get_metadata();

            // Merge values
            for (const auto& [rkey, rvalue] : other.get_values())
            {
                auto lpb = meta.values_emplace(allocator, values_, rkey, dot_kernel_value_type(allocator, rkey, this));
                auto&& lvalue = *lpb.first;

                value_context < decltype(allocator), Metadata > value_ctx(allocator, meta);
                value_dots_merge(allocator, lvalue.second.dots, rvalue.dots, value_ctx);
                lvalue.second.merge(allocator, rvalue, value_ctx);
            
                for (const auto& [replica_id, rdots] : rvalue.dots)
                {
                    // Track visited dots
                                        
                    auto& ldata = meta.get_replica_data(allocator, replica_id); // replica_.emplace(allocator, replica_id, replica_data()).first->second;
                                        
                    meta.counters_insert(tmp, visited[replica_id], rdots.begin(), rdots.end());
        
                    for (const auto& counter : rdots)
                    {
                        // Create dot -> key link
                        meta.replica_dots_add(allocator, ldata, counter, rkey);
                    }
                }

                // Support for insert / emplace pairb result
                ctx.register_insert(lpb);
            }

            // Merge replicas
            for (const auto& [replica_id, rdata] : other.get_replica_map())
            {
                // Merge global counters
                auto& ldata = meta.get_replica_data(allocator, replica_id);
                
                counters_merge(allocator, ldata.counters, replica_id, rdata.counters, counters_context());
                                
                // Determine deleted values (those are the ones we have not visited in a loop over values).

                auto it = visited.find(replica_id);
                if (it != visited.end())
                {
                    std::deque< counter_type, typename std::allocator_traits< decltype(tmp) >::template rebind_alloc< counter_type > > rdotsvalueless(tmp);

                    auto& rdata_visited = (*it).second;
                    std::set_difference(
                        rdata.counters.begin(), rdata.counters.end(),
                        rdata_visited.begin(), rdata_visited.end(),
                        std::back_inserter(rdotsvalueless)
                    );

                    meta.counters_clear(tmp, rdata_visited);

                    merge_valueless_clear(allocator, ctx, ldata, rdotsvalueless, replica_id);
                }
                else
                {                   
                    merge_valueless_clear(allocator, ctx, ldata, rdata.counters, replica_id);
                }
            }
        }

        template< typename Context, typename LData, typename Counters > void merge_valueless_clear(
            Allocator& allocator, Context& ctx, LData& ldata, Counters& counters, replica_id_type replica_id
        )
        {
            auto& meta = this->get_metadata();

            for (const auto& counter : counters)
            {
                auto counter_it = meta.replica_dots_find(ldata, counter);
                if (counter_it != ldata.dots.end())
                {
                    auto& lkey = counter_it->second;

                    auto value_it = meta.values_find(values_, lkey);
                    meta.value_counters_erase(allocator, value_it->second.dots, dot_type{ replica_id, counter });

                    if (value_it->second.dots.empty())
                    {
                        // Support for erase iterator return value
                        auto it = meta.values_erase(allocator, values_, value_it);
                        ctx.register_erase(it);
                    }

                    meta.replica_dots_erase(allocator, ldata, counter_it);
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
        const_iterator find(const Key& key) const { return this->get_metadata().values_find(values_, key); }
        iterator find(const Key& key) { return this->get_metadata().values_find(values_, key); }

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
            auto& meta = this->get_metadata();
            for (auto&& [replica_id, counters] : dots)
            {                
                meta.replica_counters_add(allocator, replica_id, counters);
            }
        }

        void add_counter_dot(const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            this->get_metadata().replica_counters_add(allocator, dot.replica_id, dot.counter);
        }

        // TODO: const
        dot_type get_next_dot()
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto replica_id = allocator.get_replica().get_id();
            return { replica_id, replica_counters_get(replica_id) + 1 };
        }

        void add_value(const Key& key, const dot_type& dot)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto& meta = this->get_metadata();
            auto&& data = *meta.values_emplace(allocator, values_, key, dot_kernel_value_type(allocator, key, nullptr)).first;
            meta.value_counters_emplace(allocator, data.second.dots, dot);
        }

        template < typename ValueT > void add_value(const Key& key, const dot_type& dot, ValueT&& value)
        {
            auto allocator = static_cast<Container*>(this)->get_allocator();
            auto& meta = this->get_metadata();
            auto&& data = *meta.values_emplace(allocator, values_, key, dot_kernel_value_type(allocator, key, nullptr)).first;
            meta.value_counters_emplace(allocator, data.second.dots, dot);
            data.second.value.merge(value);
        }
                
        const auto& get_replica_map() const { return this->get_metadata().get_replica_map(); }

        const values_type& get_values() const { return values_; }

    private:
        counter_type replica_counters_get(replica_id_type id)
        {
            counter_type counter = counter_type();
            auto& meta = this->get_metadata();
            auto replica = meta.get_replica_data(id);
            if (replica)
            {
                counter = meta.replica_counters_get(*replica);
            }

            return counter;
        }
                
                
        template < typename AllocatorT, typename LCounters, typename RCounters, typename Context > void counters_merge(AllocatorT& allocator, LCounters& lcounters, replica_id_type replica_id, RCounters& rcounters, Context&& context)
        {
            if (std::is_same_v< Tag, tag_state >)
            {
                // TODO: investigate
                // assert(counters_.size() <= 1);      // state variant should have 0 or 1 elements
            }

            // TODO: this assert should really by true. The issue is in map::insert with value_mv - it is not delta there.
            //static_assert(std::is_same_v< RCounters::tag_type, tag_delta >);
            // 
            // assert(rcounters.size() == 1);   // delta variant can have N elements, and merging two state variants does not make sense

            auto& meta = this->get_metadata();

            if (lcounters.size() == 1 && rcounters.size() == 1)
            {
                // Maybe in-place replace
                if (*lcounters.begin() + 1 == *rcounters.begin())
                {
                    auto counter = *lcounters.begin();
                    meta.counters_update(allocator, lcounters, lcounters.begin(), counter + 1);

                    // No need to collapse here, but have to notify upper layer about removal
                    context.register_erase(dot_type{ replica_id, counter });

                    return;
                }
            }

            meta.counters_insert(allocator, lcounters, rcounters.begin(), rcounters.end());

            if (std::is_same_v< Tag, tag_state >)
            {
                counters_collapse(allocator, lcounters, replica_id, context);
            }
        }

        template < typename AllocatorT, typename Counters, typename Context > void counters_collapse(AllocatorT& allocator, Counters& counters, replica_id_type replica_id, Context& context)
        {
            auto& meta = this->get_metadata();
            auto next = counters.begin();
            auto prev = next++;
            for (; next != counters.end();)
            {
                if (*next == *prev + 1)
                {
                    // TODO: we should find a largest block to erase, not erase single element and continue
                    context.register_erase(dot_type{ replica_id, *prev });
                    next = meta.counters_erase(allocator, counters, prev);
                    prev = next++;
                }
                else
                {
                    break;
                }
            }
        }

        template < typename AllocatorT, typename Dots, typename Context > void value_dots_merge(AllocatorT& allocator, typename Metadata::value_type_dots_type& ldots, const Dots& rdots, Context& context)
        {
            auto& meta = this->get_metadata();
            for (auto&& [replica_id, rcounters] : rdots)
            {
                auto& lcounters = meta.value_counters_fetch(allocator, ldots, replica_id);
                counters_merge(allocator, lcounters, replica_id, rcounters, context);
            }
        }

        values_type values_;
    };
}
