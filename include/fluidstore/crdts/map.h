#pragma once

#include <fluidstore/crdts/dot_kernel.h>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Replica > class map
        : public dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, typename Replica::counter_type, map< Key, Value, Allocator, Replica > >
        , public Replica::template hook< map< Key, Value, Allocator, Replica > >
    {
        typedef map< Key, Value, Allocator, Replica > map_type;
        typedef dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, typename Replica::counter_type, map_type > dot_kernel_type;
    
    public:
        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef map< Key, typename Value::template rebind< AllocatorT, ReplicaT >::type, AllocatorT, ReplicaT > type;
        };

        map(Allocator allocator)
            : Replica::template hook< map_type >(allocator.get_replica())
            , dot_kernel_type(allocator)
        {}

        map(Allocator allocator, typename Replica::id_type id)
            : Replica::template hook< map_type >(allocator.get_replica(), id)
            , dot_kernel_type(allocator)
        {}

        void insert(const Key& key, const Value& value)
        {
            arena< 1024 > buffer;

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename Replica::replica_id_type, typename Replica::instance_id_type, typename Replica::counter_type > rep(replica_id);
            allocator< decltype(rep) > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            map< Key, Value, decltype(allocator3), decltype(rep) > delta(allocator3, this->get_id());

            // dot_kernel_type delta(this->allocator_, this->get_id());

            // dot_kernel_map< Key, Value, AllocatorT, ReplicaT, Counter >

            // dot_kernel_type delta(this->allocator_);

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });

            auto& data = delta.values_[key];
            data.dots.emplace(dot_type{ replica_id, counter });
            data.value = value;

            this->merge(delta);
        }
    };
}