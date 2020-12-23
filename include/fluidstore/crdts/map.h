#pragma once

#include <fluidstore/crdts/dot_kernel.h>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Replica, typename Counter > class map_base
        : public dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, Counter, map_base< Key, Value, Allocator, Replica, Counter > >
        , public Replica::template hook< map_base< Key, Value, Allocator, Replica, Counter > >
    {
        typedef map_base< Key, Value, Allocator, Replica, Counter > map_base_type;

    public:
        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef map_base< Key, typename Value::template rebind< AllocatorT, ReplicaT >::type, AllocatorT, ReplicaT, Counter > type;
        };

        map_base(Allocator allocator, typename Replica::instance_id_type id)
            : dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, Counter, map_base_type >(allocator)
            , Replica::template hook< map_base_type >(allocator.get_replica(), id)
        {}

        map_base(Allocator allocator)
            : dot_kernel< Key, Value, Allocator, typename Replica::replica_id_type, Counter, map_base_type >(allocator)
            , Replica::template hook< map_base_type >(allocator.get_replica())
        {}

        void insert(const Key& key, const Value& value)
        {
            arena< 1024 > buffer;

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename Replica::replica_id_type > rep(replica_id);
            allocator< replica< typename Replica::replica_id_type > > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            map_base< Key, Value, decltype(allocator3), replica< typename Replica::replica_id_type >, Counter > delta(allocator3, this->get_id());

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

    template< typename Key, typename Value, typename Traits > class map
        : public map_base< Key, Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >
        , noncopyable
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        map(allocator_type allocator, typename Traits::instance_id_type id)
            : map_base< Key, Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator, id)
        {}

        map(std::allocator_arg_t, allocator_type allocator)
            : map_base< Key, Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator)
        {}
    };
}