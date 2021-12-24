#pragma once

#include <fluidstore/btree/set.h>
#include <fluidstore/btree/map.h>

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
//
// how to get there:
//      replica will provide next id to crdt upon creation lazily, crdts will return it when done
//      crdts will move id with move, obtain new upon construction
//      delta mutations cannot share metadata with persistent ones!!!
//      -> I will need to abstract metadata access and parametrize it. Deltas will hold 'local metadata',
//          while persistent structs will share them
//

namespace crdt
{
    namespace detail 
    {
        struct metadata_local {};
        struct metadata_shared {};

        // template < typename ReplicaId, typename InstanceId, typename Counter, typename Tag > struct metadata;

        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct metadata;

        template < typename Key, typename Tag, typename Allocator > struct metadata
            < Key, Tag, Allocator, metadata_shared >
        {
            // Instance ids
            //InstanceId acquire_instance_id();
            //void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                // TODO: this should be rewriten using InstanceId, Counter
                //btree::map_base< InstanceId, dot_counters_base< Counter, Tag >* > counters;
                //btree::map_base< std::pair< InstanceId, Counter >, Key > dots;
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

            // Values (note that this is per-Instance-per-Key)
            //btree::map_base< std::pair< InstanceId, Key >, dot_counters_base< Counter, Tag >* > value_counters;
        };

        template <  typename Key, typename Tag, typename Allocator > struct metadata
            < Key, Tag, Allocator, metadata_local >
        {
            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            // Instance ids
            // InstanceId acquire_instance_id();
            // void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                dot_counters_base< counter_type, Tag > counters;
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

            counter_type get_counter(replica_id_type id)
            {
                counter_type counter = counter_type();
                auto replica = get_replica_data(id);
                if (replica)
                {
                    counter = replica->counters.get();
                }

                return counter;
            }

            void erase_counter(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                auto replica = get_replica_data(id);
                if (replica)
                {
                    replica->dots.erase(allocator, counter);
                }
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

            // Values (note that this is per-Instance-per-Key)
            //btree::map_base< Key, dot_counters_base< Counter, Tag >* > value_counters;
        };

        template < typename T, typename Id > struct instance_base
        {
            instance_base()
                : id_()
            {}

            ~instance_base()
            {
                if (id_)
                {
                    // id_ = static_cast<T*>(this)->get_allocator().get_replica().release_instance_id(id_);
                }
            }

            Id get_id()
            {
                if (!id_)
                {
                    // id_ = static_cast<T*>(this)->get_allocator().get_replica().acquire_instance_id();
                }

                return id;
            }

        private:
            Id id_;
        };
    }
}