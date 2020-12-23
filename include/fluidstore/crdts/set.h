#pragma once

namespace crdt
{
    template < typename Key, typename Allocator, typename Replica, typename Counter > class set_base
        : public dot_kernel< Key, void, Allocator, typename Replica::replica_id_type, Counter, set_base< Key, Allocator, Replica, Counter > >
        , public Replica::template hook< set_base< Key, Allocator, Replica, Counter > >
    {
        typedef set_base< Key, Allocator, Replica, Counter > set_base_type;

    public:
        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            typedef set_base< Key, AllocatorT, ReplicaT, Counter > type;
        };

        set_base(Allocator allocator, typename Replica::id_type id)
            : dot_kernel< Key, void, Allocator, typename Replica::replica_id_type, Counter, set_base_type >(allocator)
            , Replica::template hook< set_base_type >(allocator.get_replica(), id)
        {}

        set_base(Allocator allocator)
            : dot_kernel< Key, void, Allocator, typename Replica::replica_id_type, Counter, set_base_type >(allocator)
            , Replica::template hook< set_base_type >(allocator.get_replica())
        {}

        /*std::pair< const_iterator, bool >*/ void insert(const Key& key)
        {
            arena< 1024 > buffer;

            //arena_allocator< void, Allocator > alloc(buffer, this->allocator_);
            //dot_kernel_set< Key, decltype(alloc), ReplicaId, Counter, InstanceId > delta(alloc, this->get_id());

            auto replica_id = this->allocator_.get_replica().get_id();

            replica< typename Replica::replica_id_type, uint64_t > rep(replica_id);
            allocator< replica< typename Replica::replica_id_type, uint64_t > > allocator2(rep);
            arena_allocator< void, decltype(allocator2) > allocator3(buffer, allocator2);
            set_base< Key, decltype(allocator3), replica< typename Replica::replica_id_type, uint64_t >, Counter > delta(allocator3, this->get_id());

            // set_base_type delta(this->allocator_, this->get_id());

            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(dot_type{ replica_id, counter });
            delta.values_[key].dots.emplace(dot_type{ replica_id, counter });

            this->merge(delta);
        }
    };

    template< typename Key, typename Traits > class set
        : public set_base< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >
        , noncopyable
    {
    public:
        typedef typename Traits::allocator_type allocator_type;

        set(allocator_type allocator, typename Traits::id_type id)
            : set_base< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator, id)
        {}

        set(std::allocator_arg_t, allocator_type allocator)
            : set_base< Key, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >(allocator)
        {}
    };
}