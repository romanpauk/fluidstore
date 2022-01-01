#pragma once

#include <map>
#include <set>

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata.h>

namespace crdt
{
    namespace detail
    {
        struct metadata_tag_stl_local {};

        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct dot_kernel_metadata;

        template <  typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, metadata_tag_stl_local >
        {
            using metadata_tag_type = metadata_tag_stl_local;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using counters_type = std::set< counter_type >;
            using value_type_dots_type = std::map < replica_id_type, counters_type >;
                        
            template < typename Key, typename Value > using values_map_type = std::map< Key, Value, std::less< Key >
                , typename std::allocator_traits< Allocator >::template rebind_alloc< std::pair< const Key, Value > >
            >;                

            // Instance ids
            // InstanceId acquire_instance_id();
            // void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                using allocator_type = Allocator;
                using counters_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< counter_type >;
                using dots_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< std::pair< const counter_type, Key > >;

                template< typename AllocatorT > replica_data(AllocatorT&& allocator)
                    : counters(allocator)
                    , visited(allocator)
                    , dots(allocator)
                {}

                std::set< counter_type, std::less< counter_type >, counters_allocator_type > counters;
                std::map< counter_type, Key, std::less< counter_type >, dots_allocator_type > dots;
                std::set< counter_type, std::less< counter_type >, counters_allocator_type > visited;
            };

            using replica_map_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc < std::pair< const replica_id_type, replica_data > >;
            using replica_map_type = std::map < replica_id_type, replica_data, std::less< replica_id_type >, replica_map_allocator_type >;

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

                get_replica_data(allocator, id).counters.emplace(counter);
            }

            template < typename Counters > void replica_counters_add(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                get_replica_data(allocator, id).counters.insert(counters.begin(), counters.end());
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

            template < typename Key > void replica_dots_add(Allocator&, replica_data& replica, counter_type counter, Key key)
            {
                replica.dots.emplace(counter, key);
            }

            void replica_dots_erase(Allocator&, replica_id_type id, counter_type counter)
            {
                auto replica = get_replica_data(id);
                if (replica)
                {
                    replica->dots.erase(counter);
                }
            }

            void replica_dots_erase(Allocator&, replica_data& replica, typename std::map< counter_type, Key >::iterator it)
            {
                replica.dots.erase(it);
            }

            auto replica_dots_find(replica_data& replica, counter_type counter)
            {
                return replica.dots.find(counter);
            }

            auto& value_counters_fetch(Allocator&, value_type_dots_type& ldots, replica_id_type id)
            {
                auto& counters = ldots.emplace(id, std::set< counter_type >()).first->second;
                return counters;
            }

            void value_counters_emplace(Allocator&, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto& counters = ldots.emplace(dot.replica_id, std::set< counter_type >()).first->second;
                counters.emplace(dot.counter);
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

            template < typename Values, typename Key, typename Value > auto values_emplace(Allocator&, Values& values, const Key& key, Value&& value)
            {
                return values.emplace(key, std::forward< Value >(value));
            }

            template < typename Values > auto values_find(Values& values, const Key& key)
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

        template < typename Container, typename Metadata > struct dot_kernel_metadata_base< Container, Metadata, metadata_tag_stl_local >
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