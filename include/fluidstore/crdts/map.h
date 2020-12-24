#pragma once

#include <fluidstore/crdts/dot_kernel.h>

#include <memory>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator > class map
        : public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator > >
        , public Allocator::replica_type::template hook< map< Key, Value, Allocator > >
    {
        typedef map< Key, Value, Allocator > map_type;
        typedef dot_kernel< Key, Value, Allocator, map_type > dot_kernel_type;
    
    public:
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;

        template < typename AllocatorT > struct rebind
        {
            typedef map< Key, typename Value::template rebind< AllocatorT >::type, AllocatorT > type;
        };

        map(Allocator allocator)
            : replica_type::template hook< map_type >(allocator.get_replica())
            , dot_kernel_type(allocator)
        {}

        map(Allocator allocator, typename Allocator::replica_type::id_type id)
            : replica_type::template hook< map_type >(allocator.get_replica(), id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            arena< 1024 * 2 > buffer;

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename replica_type::replica_id_type, typename replica_type::instance_id_type, typename replica_type::counter_type > rep(replica_id, this->allocator_.get_replica().get_sequence());
            allocator< decltype(rep) > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            map< Key, typename Value::template rebind< decltype(allocator3) >::type, decltype(allocator3) > delta(allocator3, this->get_id());

            //map_type delta(this->allocator_, this->get_id());

            // dot_kernel_map< Key, Value, AllocatorT, ReplicaT, Counter >

            // dot_kernel_type delta(this->allocator_);

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });
            auto& data = delta.values_[key];
            data.dots.emplace(dot_type{ replica_id, counter });
            data.value.merge(value);

            // TODO:
            // When this is insert of one element, merge should change internal state only when an insert operation would.
            merge_info info;
            this->merge(delta, &info);
            this->allocator_.merge(*this, delta);

            return { info.iterator, info.inserted };
        }

        Value& operator[](const Key& key)
        {
            if constexpr (std::uses_allocator_v< Value, Allocator >)
            {
                // TODO: this Value should not be tracked, it is temporary.
                auto pairb = insert(key, Value(allocator_));
                return (*pairb.first).second;
            }
            else
            {
                auto pairb = insert(key, Value());
                return (*pairb.first).second;
            }
        }
    };
}