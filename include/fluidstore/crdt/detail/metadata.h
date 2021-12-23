#pragma once


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
        template < typename ReplicaId, typename InstanceId, typename Counter, typename Tag > struct metadata
        {
            // Instance ids
            InstanceId acquire_instance_id();
            void release_instance_id(InstanceId);

            // Replicas
            struct replica_data
            {
                btree::map_base< InstanceId, dot_counters_base< Counter, Tag >* > counters;
                btree::map_base< std::pair< InstanceId, Counter >, Key > dots;
            };

            btree::map_base< ReplicaId, replica_data > replicas;

            // Values (note that this is per-Instance-per-Key)
            btree::map_base< std::pair< InstanceId, Key >, dot_counters_base* > value_counters;
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