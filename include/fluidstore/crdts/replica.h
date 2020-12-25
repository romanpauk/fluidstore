#pragma once

#include <fluidstore/crdts/noncopyable.h>
#include <fluidstore/crdts/id_sequence.h>

namespace crdt
{
    template < typename ReplicaId = uint64_t, typename InstanceId = uint64_t, typename Counter = uint64_t > class replica
        : noncopyable
    {
        template < typename ReplicaIdT, typename InstanceIdT, typename CounterT > friend class replica;

    public:
        typedef replica< ReplicaId, InstanceId, Counter > replica_type;
        typedef replica_type delta_replica_type;
        typedef ReplicaId replica_id_type;
        typedef InstanceId instance_id_type;
        typedef Counter counter_type;
        typedef std::pair< ReplicaId, InstanceId > id_type;
        typedef id_sequence< InstanceId > id_sequence_type;

        template < typename Instance > struct hook
        {
            hook(replica_type& replica)
                : id_(replica.generate_instance_id())
            {}

            hook(replica_type&, const id_type& id)
                : id_(id)
            {}

            const id_type& get_id() const { return id_; }

        private:
            id_type id_;
        };

        replica(const ReplicaId& id, id_sequence< InstanceId >& seq)
            : id_(id)
            , sequence_(seq)
        {}

        template < typename ReplicaT > replica(ReplicaT& other)
            : id_(other.id_)
            , sequence_(other.sequence_)
        {}

        const ReplicaId& get_id() const { return id_; }
        id_type generate_instance_id() { return { id_, sequence_.next() }; }
        id_sequence_type& get_sequence() { return sequence_; }

        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta) {}

    private:
        ReplicaId id_;
        id_sequence< InstanceId >& sequence_;
    };
}