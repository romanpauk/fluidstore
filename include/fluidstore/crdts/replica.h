#pragma once

#include <fluidstore/crdts/noncopyable.h>
#include <fluidstore/crdts/id_sequence.h>

namespace crdt
{
    // Shared between all replicas. Contracts for IDs.
    template < typename ReplicaId = uint64_t, typename InstanceId = uint64_t, typename Counter = uint64_t > class system
    {
    public:
        typedef ReplicaId replica_id_type;
        typedef InstanceId instance_id_type;
        typedef id_sequence< InstanceId > instance_id_sequence_type;
        typedef Counter counter_type;
        typedef std::pair< replica_id_type, instance_id_type > id_type;
    };

    template < typename System = crdt::system<> > class replica
        : noncopyable
        , public System
    {
        template < typename System > friend class replica;

    public:
        typedef System system_type;
        typedef replica< System > replica_type;
        typedef replica_type delta_replica_type;
        
        using typename system_type::replica_id_type;
        using typename system_type::instance_id_type;
        using typename system_type::instance_id_sequence_type;
        using typename system_type::counter_type;
        using typename system_type::id_type;

        template < typename Instance > struct hook
        {
            hook(const id_type& id)
                : id_(id)
            {}

            const id_type& get_id() const { return id_; }

        private:
            id_type id_;
        };

        replica(const replica_id_type& id, instance_id_sequence_type& sequence)
            : id_(id)
            , instance_id_sequence_(sequence)
        {}

        template < typename ReplicaT > replica(ReplicaT& other)
            : id_(other.id_)
            , instance_id_sequence_(other.sequence_)
        {}

        const replica_id_type& get_id() const { return id_; }
        id_type generate_instance_id() { return { id_, instance_id_sequence_.next() }; }
        
        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

    private:
        replica_id_type id_;
        instance_id_sequence_type& instance_id_sequence_;
    };
}