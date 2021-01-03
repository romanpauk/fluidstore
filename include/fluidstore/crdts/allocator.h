#pragma once

#include <fluidstore/crdts/replica.h>

#include <memory>

namespace crdt
{
    template < typename T > struct allocator_container 
    {
        allocator_container() {}
        T* ptr_;
    };

    template<> struct allocator_container<void> {};

    template <
        typename Replica = replica<>,
        typename T = unsigned char,
        typename Allocator = std::allocator< T >,
        typename Container = allocator_container< void >
    > class allocator
        : public Allocator
        , public Container
    {
        template < typename Replica, typename U, typename AllocatorU, typename ContainerU > friend class allocator;

    public:
        typedef Replica replica_type;

        template< typename U, typename ContainerU = Container > struct rebind
        {
            using other = allocator< Replica, U, typename Allocator::template rebind< U >::other, ContainerU >;
        };

        allocator(Replica& replica)
            : replica_(replica)
        {}

        template < typename ReplicaU, typename U, typename AllocatorU, typename ContainerU > allocator(const allocator< ReplicaU, U, AllocatorU, ContainerU >& other)
            : Allocator(other)
            , replica_(other.replica_)
        {}

        auto& get_replica() const { return replica_; }

    private:
        Replica& replica_;
    };
}