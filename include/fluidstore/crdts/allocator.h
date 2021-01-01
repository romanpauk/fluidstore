#pragma once

#include <fluidstore/crdts/replica.h>

#include <memory>

namespace crdt
{
    template <
        typename Replica = replica<>,
        typename T = unsigned char,
        typename Allocator = std::allocator< T >
    > class allocator
        : public Allocator
    {
        template < typename Replica, typename U, typename AllocatorU > friend class allocator;

    public:
        typedef Replica replica_type;

        template< typename U > struct rebind
        {
            using other = allocator< Replica, U, typename Allocator::template rebind< U >::other >;
        };

        allocator(Replica& replica)
            : replica_(replica)
        {}

        template < typename ReplicaU, typename U, typename AllocatorU > allocator(const allocator< ReplicaU, U, AllocatorU >& other)
            : Allocator(other)
            , replica_(other.replica_)
        {}

        auto& get_replica() const { return replica_; }

    private:
        Replica& replica_;
    };
}