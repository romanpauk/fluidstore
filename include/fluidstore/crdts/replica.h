#pragma once

#include <fluidstore/crdts/noncopyable.h>

namespace crdt
{
    template < typename ReplicaIdType = uint64_t, typename CounterType = uint64_t > class replica
    {
    public:
        using replica_type = replica< ReplicaIdType >;
        using replica_id_type = ReplicaIdType;
        using counter_type = CounterType;

        replica(const replica_id_type& id)
            : id_(id)
        {}

        replica(const replica&) = default;
            
        const replica_id_type& get_id() const { return id_; }
        
    private:
        replica_id_type id_;
    };
}