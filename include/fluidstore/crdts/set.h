#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/allocators/arena_allocator.h>

namespace crdt
{
    template < typename Key, typename Allocator > class set
        : public dot_kernel< Key, void, Allocator, set< Key, Allocator > >
        , public Allocator::replica_type::template hook< set< Key, Allocator > >
    {
        typedef set< Key, Allocator > set_type;
        typedef dot_kernel< Key, void, Allocator, set_type > dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;

        template < typename AllocatorT > struct rebind { typedef set< Key, AllocatorT > type; };

        set(Allocator allocator)
            : replica_type::template hook< set_type >(allocator.get_replica())
            , dot_kernel_type(allocator)
        {}

        set(Allocator allocator, typename Allocator::replica_type::id_type id)
            : replica_type::template hook< set_type >(allocator.get_replica(), id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            arena< 1024 > buffer;

            //arena_allocator< void, Allocator > alloc(buffer, this->allocator_);
            //dot_kernel_set< Key, decltype(alloc), ReplicaId, Counter, InstanceId > delta(alloc, this->get_id());

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename replica_type::replica_id_type, typename replica_type::instance_id_type, typename replica_type::counter_type > rep(replica_id, this->allocator_.get_replica().get_sequence());
            allocator< decltype(rep) > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            set< Key, decltype(allocator3) > delta(allocator3, this->get_id());

            // set_base_type delta(this->allocator_, this->get_id());

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });
            delta.values_[key].dots.emplace(dot_type{ replica_id, counter });

            merge_context< merge_result > context;
            this->merge(delta, &context);
            this->allocator_.merge(*this, delta);

            return { context.iterator, context.inserted };
        }
    };
}