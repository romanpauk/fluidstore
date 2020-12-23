#pragma once

#include <tuple>
#include <functional>

namespace crdt
{
    template < typename ReplicaId, typename Counter > class dot
    {
    public:
        ReplicaId replica_id;
        Counter counter;

        bool operator < (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) < std::make_tuple(other.replica_id, other.counter); }
        bool operator > (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) > std::make_tuple(other.replica_id, other.counter); }
        bool operator == (const dot< ReplicaId, Counter >& other) const { return std::make_tuple(replica_id, counter) == std::make_tuple(other.replica_id, other.counter); }
    
        size_t hash() const
        {
            std::size_t h1 = std::hash< ReplicaId >{}(replica_id);
            std::size_t h2 = std::hash< Counter >{}(counter);
            return h1 ^ (h2 << 1);
        }
    };
}

namespace std
{
    template< typename ReplicaId, typename Counter > struct hash< crdt::dot< ReplicaId, Counter > >
    {
        std::size_t operator()(const crdt::dot< ReplicaId, Counter >& dot) const noexcept
        {
            return dot.hash();
        }
    };
}
