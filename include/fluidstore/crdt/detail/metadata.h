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
        struct tag_local {};
        struct tag_shared {};

        // template < typename ReplicaId, typename InstanceId, typename Counter, typename Tag > struct metadata;

        template < typename Key, typename Tag, typename Allocator, typename MetadataTag > struct metadata;

        // TODO: Shared metadata will use shared allocator
        //  Local metadata should local allocator.
        // 
        // What to do with Key/Tag? Replica that will own metadata for shared will not know them.
        // Shared metadata will need to have support for dynamic creation of keys/tags sets.

        template < typename Key, typename Tag, typename Allocator > struct metadata
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
                
        template <  typename Key, typename Tag, typename Allocator > struct metadata
            < Key, Tag, Allocator, tag_local >
        {
            using metadata_tag_type = tag_local;

            using allocator_type = Allocator;
            using replica_type = typename allocator_type::replica_type;
            using replica_id_type = typename replica_type::replica_id_type;
            using counter_type = typename replica_type::counter_type;

            using value_type_dots_type = btree::map_base < replica_id_type, btree::set_base< counter_type > >;
            using dot_context_type = dot_context< dot< replica_id_type, counter_type >, Tag >;

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

            counter_type get_counter(replica_id_type id)
            {
                counter_type counter = counter_type();
                auto replica = get_replica_data(id);
                if (replica)
                {
                    counter = dot_counters_base< counter_type, Tag, size_t >(replica->counters).get();
                }

                return counter;
            }

            void add_counter(Allocator& allocator, replica_id_type id, counter_type counter)
            {
                dot_counters_base< counter_type, Tag, size_t >(get_replica_data(allocator, id).counters).emplace(allocator, counter);
            }

            template < typename Counters > void add_counters(Allocator& allocator, replica_id_type id, Counters& counters)
            {
                dot_counters_base< counter_type, Tag, size_t >(get_replica_data(allocator, id).counters).insert(allocator, counters);
            }

            template < typename ReplicaData > void merge_counters(Allocator& allocator, replica_data& lhs, replica_id_type id, ReplicaData& rhs)
            {
                dot_counters_base< counter_type, Tag, size_t >(lhs.counters).merge(allocator, id, rhs.counters);
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

            template < typename Dots, typename Context > void merge_value_dots(Allocator& allocator, value_type_dots_type& ldots, const Dots& rdots, Context& context)
            {
                dot_context_type(ldots).merge(allocator, rdots, context);
            }

            void emplace_value_dot(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                dot_context_type(ldots).emplace(allocator, dot);
            }

            void erase_value_dot(Allocator& allocator, value_type_dots_type& ldots, dot< replica_id_type, counter_type > dot)
            {
                dot_context_type(ldots).erase(allocator, dot);
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

        template < typename Container, typename Metadata, typename MetadataTag = typename Metadata::metadata_tag_type > struct metadata_base;
        
        template < typename Container, typename Metadata > struct metadata_base< Container, Metadata, tag_local >
        {            
            Metadata& get_metadata() { return metadata_; }
            const Metadata& get_metadata() const { return metadata_; }

        private:
            Metadata metadata_;
        };
        
        template < typename Container, typename Metadata > struct metadata_base< Container, Metadata, tag_shared >
        {
            Metadata& get_metadata() { return static_cast< Container* >(this)->get_replica().get_metadata(); }
            const Metadata& get_metadata() const { return return static_cast<Container*>(this)->get_replica().get_metadata(); }
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