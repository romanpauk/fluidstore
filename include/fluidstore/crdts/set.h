#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/allocators/arena_allocator.h>

namespace crdt
{
    template < typename Key, typename Allocator > class set
        : public dot_kernel< Key, void, Allocator, set< Key, Allocator > >
        , public Allocator::replica_type::template hook< set< Key, Allocator > >
        , public Allocator::template hook< set< Key, Allocator > >
    {
        typedef set< Key, Allocator > set_type;
        typedef dot_kernel< Key, void, Allocator, set_type > dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        typedef typename Allocator::replica_type replica_type;

        template < typename AllocatorT > struct rebind { typedef set< Key, AllocatorT > type; };

        set(Allocator allocator)
            : replica_type::template hook< set_type >(allocator.get_replica(), allocator.get_replica().generate_instance_id())
            , allocator_type::template hook< set_type >()
            , dot_kernel_type(allocator)
        {}

        set(Allocator allocator, typename Allocator::replica_type::id_type id)
            : replica_type::template hook< set_type >(allocator.get_replica(), id)
            , allocator_type::template hook< set_type >()
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            arena< 1024 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->allocator_.get_replica());
            typename rebind< decltype(allocator) >::type delta(allocator, this->get_id());
            // set_type delta(this->allocator_, this->get_id());

            insert(delta, key);
         
            insert_context context;
            this->merge(delta, context);
            this->allocator_.merge(*this, delta);
            return { context.result.first, context.result.second };
        }

        template < typename Delta > void insert(Delta& delta, const Key& key)
        {
            auto replica_id = this->allocator_.get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(replica_id, counter);
            delta.values_[key].dots.emplace(replica_id, counter);
        }
    };
}