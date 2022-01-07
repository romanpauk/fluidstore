#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata.h>

namespace crdt
{
    namespace detail
    {
        struct metadata_tag_flat_local {};

        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct dot_kernel_metadata;

        template <  typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, metadata_tag_flat_local >
        {
            using metadata_tag_type = metadata_tag_flat_local;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using counters_type = boost::container::flat_set< counter_type >;
            using value_type_dots_type = boost::container::flat_map< replica_id_type, counters_type >;
                        
            template < typename KeyT, typename ValueT > using values_map_type = boost::container::flat_map< 
                KeyT, 
                ValueT, 
                std::less< KeyT >,
                typename std::allocator_traits< Allocator >::template rebind_alloc< std::pair< const KeyT, ValueT > >
            >;                

            template < typename AllocatorT > using visited_map_type = boost::container::flat_map<
                replica_id_type,
                boost::container::flat_set< counter_type >,
                std::less< replica_id_type >,
                typename std::allocator_traits< AllocatorT >::template rebind_alloc< std::pair< const replica_id_type, boost::container::flat_set< counter_type > > >
            >;
                        
            // Replicas
            struct replica_data
            {
                using allocator_type = Allocator;
                using counters_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< counter_type >;
                using dots_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< std::pair< const counter_type, Key > >;

                template< typename AllocatorT > replica_data(AllocatorT&& allocator)
                    : counters(allocator)
                    , dots(allocator)
                {}

                boost::container::flat_set< counter_type, std::less< counter_type >, counters_allocator_type > counters;
                boost::container::flat_map< counter_type, Key, std::less< counter_type >, dots_allocator_type > dots;
            };

            using replica_map_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc < std::pair< const replica_id_type, replica_data > >;
            using replica_map_type = boost::container::flat_map < replica_id_type, replica_data, std::less< replica_id_type >, replica_map_allocator_type >;

            dot_kernel_metadata(Allocator& allocator)
                : replica_(replica_map_allocator_type(allocator))
            {}

            replica_data& get_replica_data(Allocator&, replica_id_type id)
            {
                return replica_.emplace(id, replica_data(replica_.get_allocator())).first->second;
            }

            replica_data* get_replica_data(replica_id_type id)
            {
                auto it = replica_.find(id);
                return it != replica_.end() ? &it->second : nullptr;
            }

            counter_type replica_counters_get(replica_data& replica)
            {
                return !replica.counters.empty() ? *--replica.counters.end() : counter_type();
            }

            void replica_counters_add(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                auto& counters = get_replica_data(allocator, id).counters;
                counters.emplace_hint(counters.end(), counter);
            }

            template < typename Counters > void replica_counters_add(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                auto& lcounters = get_replica_data(allocator, id).counters;
                lcounters.insert(counters.begin(), counters.end());
            }

            template < typename Counters > auto counters_erase(Allocator&, Counters& counters, typename counters_type::iterator it)
            {
                return counters.erase(it);
            }

            template < typename Counters > void counters_update(Allocator&, Counters& counters, typename Counters::iterator it, counter_type value)
            {
                // TODO: in-place update, this should not change the tree layout
                counters.insert(it, value);
                counters.erase(it);
            }

            template < typename AllocatorT, typename Counters, typename It > void counters_insert(AllocatorT&, Counters& counters, It begin, It end)
            {
                counters.insert(begin, end);
            }

            template < typename AllocatorT, typename Counters > void counters_clear(AllocatorT&, Counters& counters)
            {
                counters.clear();
            }

            void replica_dots_add(Allocator&, replica_data& replica, counter_type counter, Key key)
            {
                replica.dots.emplace_hint(replica.dots.end(), counter, key);
            }

            void replica_dots_erase(Allocator&, replica_id_type id, counter_type counter)
            {
                auto replica = get_replica_data(id);
                if (replica)
                {
                    replica->dots.erase(counter);
                }
            }

            template < typename It > void replica_dots_erase(Allocator&, replica_data& replica, It it)
            {
                replica.dots.erase(it);
            }

            auto replica_dots_find(replica_data& replica, counter_type counter)
            {
                return replica.dots.find(counter);
            }

            auto& value_counters_fetch(Allocator&, value_type_dots_type& ldots, replica_id_type id)
            {
                auto& counters = ldots.emplace(id, boost::container::flat_set< counter_type >()).first->second;
                return counters;
            }

            void value_counters_emplace(Allocator&, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto& counters = ldots.emplace(dot.replica_id, boost::container::flat_set< counter_type >()).first->second;
                counters.emplace_hint(counters.end(), dot.counter);
            }

            void value_counters_erase(Allocator&, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto it = ldots.find(dot.replica_id);
                if (it != ldots.end())
                {
                    it->second.erase(dot.counter);
                    if (it->second.empty())
                    {
                        ldots.erase(it);
                    }
                }
            }

            template < typename Values, typename ValueT > auto values_emplace(Allocator&, Values& values, Key key, ValueT&& value)
            {
                return values.emplace(key, std::forward< ValueT >(value));
            }

            template < typename Values > auto values_find(Values& values, Key key)
            {
                return values.find(key);
            }

            template < typename Values > auto values_erase(Allocator&, Values& values, typename Values::iterator it)
            {
                return values.erase(it);
            }

            template < typename Values > void values_clear(Allocator&, Values&) {}

            const replica_map_type& get_replica_map() const { return replica_; }
            replica_map_type& get_replica_map() { return replica_; }

            void clear(Allocator&)
            {
                replica_.clear();
            }

        private:
            replica_map_type replica_;
        };

        template < typename Container, typename Metadata > struct dot_kernel_metadata_base< Container, Metadata, metadata_tag_flat_local >
        {
            // TODO: the allocator should live here, not in a hook.
            dot_kernel_metadata_base()
                : metadata_(static_cast<Container*>(this)->get_allocator())
            {}

            Metadata& get_metadata() { return metadata_; }
            const Metadata& get_metadata() const { return metadata_; }

        private:
            Metadata metadata_;
        };

    }
}