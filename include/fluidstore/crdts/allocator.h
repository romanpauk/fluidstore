#pragma once

#include <fluidstore/crdts/replica.h>

#include <memory>

namespace crdt
{
    template <
        typename Replica = replica<>
        , typename T = unsigned char
        , typename Allocator = std::allocator< T >
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

        template < typename Al > allocator(Replica& replica, Al&& al)
            : Allocator(std::forward<Al>(al))
            , replica_(replica)
        {}
                
        template < typename ReplicaU, typename U, typename AllocatorU > allocator(
            const allocator< ReplicaU, U, AllocatorU >& other
        )
            : Allocator(other)
            , replica_(other.replica_)
        {}
        
        allocator(const allocator< Replica, T, Allocator >& other)
            : Allocator(static_cast<const Allocator&>(other))
            , replica_(other.replica_)
        {}

        allocator(allocator< Replica, T, Allocator >&& other)
            : Allocator(std::move(static_cast< allocator< Replica, T, Allocator >& >(other)))
            , replica_(std::move(other.replica_))
        {}

        allocator< Replica, T, Allocator >& operator = (allocator< Replica, T, Allocator >&& other)
        {
            static_cast<Allocator&>(*this) = std::move(other);
            replica_ = std::move(other.replica_);
            return *this;
        }

        auto& get_replica() const { return replica_; }

        void update() {}

    private:
        Replica& replica_;
    };
}