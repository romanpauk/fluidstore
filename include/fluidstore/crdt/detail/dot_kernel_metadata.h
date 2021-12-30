#pragma once

#include <fluidstore/btree/set.h>
#include <fluidstore/btree/map.h>

#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/crdt/detail/metadata.h>

//
// dot_kernel_value
//      dot_context - map< replica, set< counter > >
//          - few replicas
//      - N*40 - move to pointer
//      = N*8
//
// dot_kernel
//      replicas_ 
//          40 counters        - collapsed counters
//              - few
//          40 dots            - counter -> key map
//              - a lot, for all keys
//          40 visited          - temporary, should be removed
//          = 8
//      - there will always be all replicas, but not full nodes
//      - N*120
//      = N*8
//         
//      values_             - key -> dot_kernel_value
//      - there will be a lot values (and a lot of non-full nodes)
//      - N*(8*40)
//      = N*8
//
// need to share common data across different data structures
//
//   map< replica_id, ...
//      set< (instance_id, counter) >   // in practice, this is more like FIFO
//      // map< (instance_id), FIFO of counters >
//      map< (instance_id, dot), key >
//      - instances merged together
//
//   map< (instance_id, key), dot_kernel_value_metadata* >?
// 
// dot_kernel
//      8 instance_id
//      8 metadata_*
//      8 values_*;
//   maybe 8 keyids_*
//
//
// how to get there:
//      replica will provide next id to crdt upon creation lazily, crdts will return it when done
//      crdts will move id with move, obtain new upon construction
//      delta mutations cannot share metadata with persistent ones!!!
//      -> I will need to abstract metadata access and parametrize it. Deltas will hold 'local metadata',
//          while persistent structs will share them
//      get rid of dot_counters_base, have just set and write algorithm elsewhere
//          when btree::set of tuples will create SoA, it will be possible to work with slice of the set
//          effectively (will work just with different SoA level).
//          btree::set< tuple< ReplicaId, Counter > > x;
//          btree::slice::set< decltype(x), 1, Counter > xx(x, replica_id);

namespace crdt
{
    namespace detail
    {
        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct dot_kernel_metadata;

        // TODO: Shared metadata will use shared allocator
        //  Local metadata should local allocator.
        // 
        // What to do with Key/Tag? Replica that will own metadata for shared will not know them.
        // Shared metadata will need to have support for dynamic creation of keys/tags sets.

        template < typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, tag_shared >
        {
            // Instance ids
            //InstanceId acquire_instance_id();
            //void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                // What to do with Key type?
                // 1. can have map from incrementing global id to Key on crdt struct
                //   (that will always append). This way, key stay in topmost layer.
                //   Price to pay is another non-shareable map per crdt instance.
                //   But it might help me going forward and get shared metadata working quicker.
                // 2. can be dynamically allocated in shared map, keyed by Key type.
                //    each Key type will get int assigned and each value will be
                //    pointer to concrete map. it will have to be virtual to be deleted.

                // TODO: will need a view over range fixed by InstanceId.
                //btree::set_base< std::tuple< InstanceId, Counter > > counters;
                //btree::map_base< std::tuple< InstanceId, Counter >, Key > dots;
                //btree::set_base< std::tuple< InstanceId, Key, ReplicaId, Counter > > value_dots;
            };

            //replica_data& get_replica_data(ReplicaId);

            //void add_dot(replica_data::dots_type&, InstanceId, Counter, Key);
            //replica_data::dots_type::iterator find_dot(replica_data::dots_type&, InstanceId, Counter);
            //auto erase_dot(replica_data::dots_type::iterator);
            //Key get_key(replica_data::dots_type::iterator);

            //void merge_counters(replica_data::counters_type&, InstanceId);

            //void clear_instance(InstanceId);

        private:
            //btree::map_base< ReplicaId, replica_data > replicas;
        };

        template <  typename Key, typename Tag, typename Allocator > struct dot_kernel_metadata
            < Key, Tag, Allocator, tag_local >
        {
            using metadata_tag_type = tag_local;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using counters_type = btree::set_base< counter_type >;
            using value_type_dots_type = btree::map_base < replica_id_type, counters_type >;            

            template < typename Key, typename Value > using values_map_type = btree::map_base< Key, Value >;

            // Instance ids
            // InstanceId acquire_instance_id();
            // void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                btree::set_base< counter_type > counters;
                btree::map_base< counter_type, Key > dots;
                btree::set_base< counter_type > visited;
            };

            replica_data& get_replica_data(Allocator& allocator, replica_id_type id)
            {
                return replica_.emplace(allocator, id, replica_data()).first->second;
            }

            replica_data* get_replica_data(replica_id_type id)
            {
                auto it = replica_.find(id);
                return it != replica_.end() ? &it->second : nullptr;
            }

            counter_type get_counter(replica_data& replica)
            {
                return !replica.counters.empty() ? *--replica.counters.end() : counter_type();
            }

            void add_counter(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                get_replica_data(allocator, id).counters.emplace(allocator, counter);
            }
                        
            template < typename Counters > void add_counters(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                if (std::is_same_v< Tag, tag_state >)
                {
                    assert(get_replica_data(allocator, id).counters.empty());
                }

                get_replica_data(allocator, id).counters.insert(allocator, counters.begin(), counters.end());                
            }            

            template < typename Key > void add_dot(Allocator& allocator, replica_data& replica, counter_type counter, Key key)
            {
                replica.dots.emplace(allocator, counter, key);
            }

            void erase_dot(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                auto replica = get_replica_data(id);
                if (replica)
                {
                    replica->dots.erase(allocator, counter);
                }
            }

            void erase_dot(Allocator& allocator, replica_data& replica, typename btree::map_base< counter_type, Key >::iterator it)
            {
                replica.dots.erase(allocator, it);
            }

            auto find_dot(replica_data& replica, counter_type counter)
            {
                return replica.dots.find(counter);
            }

            /*
            template < typename Dots, typename Context > void merge_value_dots(Allocator& allocator, value_type_dots_type& ldots, const Dots& rdots, Context& context)
            {
                dot_context_type(ldots).merge(allocator, rdots, context);
            }
            */

            void emplace_value_dot(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto& counters = ldots.emplace(allocator, dot.replica_id, btree::set_base< counter_type >()).first->second;
                counters.emplace(allocator, dot.counter);
            }

            void erase_value_dot(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                auto it = ldots.find(dot.replica_id);
                if (it != ldots.end())
                {
                    it->second.erase(allocator, dot.counter);
                    if (it->second.empty())
                    {
                        ldots.erase(allocator, it);
                    }
                }                
            }

            template < typename Values, typename Key, typename Value > auto emplace_value(Allocator& allocator, Values& values, const Key& key, Value&& value)
            {
                return values.emplace(allocator, key, std::forward< Value >(value));
            }

            template < typename Values > auto find_value(Values& values, const Key& key)
            {
                return values.find(key);
            }

            template < typename Values > auto erase_value(Allocator& allocator, Values& values, typename Values::iterator it)
            {
                return values.erase(allocator, it);
            }

            const btree::map_base < replica_id_type, replica_data >& get_replica_map() const { return replica_; }
            btree::map_base < replica_id_type, replica_data >& get_replica_map() { return replica_; }

            void clear(Allocator& allocator)
            {
                for (auto& [replica_id, data] : replica_)
                {
                    data.counters.clear(allocator);
                    data.dots.clear(allocator);
                }
                replica_.clear(allocator);
            }

        private:
            btree::map_base < replica_id_type, replica_data > replica_;

            // TODO:
            // dot_counters_base need to be modeled by (ReplicaId, Counter) and the whole map as
            // btree::set< std::tuple< Key, ReplicaId, Counter > > value_dots;
            //
            // btree::map_base< Key, dot_context< dot_type, Tag >* > value_dots;
        };
    }
}