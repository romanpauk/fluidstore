#pragma once

#include <tuple>
#include <functional>

namespace crdt
{
    template < typename ReplicaId, typename Counter > class dot
    {
    public:
        using replica_id_type = ReplicaId;
        using counter_type = Counter;

        replica_id_type replica_id;
        counter_type counter;

        bool operator < (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) < std::make_tuple(other.replica_id, other.counter); }
        bool operator <= (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) <= std::make_tuple(other.replica_id, other.counter); }
        bool operator > (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) > std::make_tuple(other.replica_id, other.counter); }
        bool operator == (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) == std::make_tuple(other.replica_id, other.counter); }
    };
}
