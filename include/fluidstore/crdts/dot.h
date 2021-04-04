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

namespace std
{
    template< typename ReplicaId, typename Counter > struct hash< crdt::dot< ReplicaId, Counter > >
    {
        std::size_t operator()(const crdt::dot< ReplicaId, Counter >& dot) const noexcept
        {
            std::size_t h1 = std::hash< ReplicaId >{}(dot.replica_id);
            std::size_t h2 = std::hash< Counter >{}(dot.counter);
            return h1 ^ (h2 << 1);
        }
    };
}
