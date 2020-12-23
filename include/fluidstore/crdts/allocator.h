#pragma once

#include <memory>

namespace crdt
{
    template < typename Replica, typename T = unsigned char, typename Allocator = std::allocator< T > > class allocator
        : public Allocator
    {
    public:
        template < typename Replica, typename U, typename AllocatorU > friend class allocator;

        template< typename U > struct rebind
        {
            typedef allocator< Replica, U,
                typename std::allocator_traits< Allocator >::template rebind_alloc< U >
            > other;
        };

        template< typename R, typename U > struct rebind_replica { typedef allocator< R, U >::type; };

        template < typename... Args > allocator(Replica& replica, Args&&... args)
            : Allocator(std::forward< Args >(args)...)
            , replica_(replica)
        {}

        template < typename U, typename AllocatorU > allocator(const allocator< Replica, U, AllocatorU >& other)
            : Allocator(other)
            , replica_(other.replica_)
        {}

        auto& get_replica() const { return replica_; }

        template < typename Instance, typename DeltaInstance > void merge(const Instance& instance, const DeltaInstance& delta_instance)
        {
            replica_.merge(instance, delta_instance);
        }

    private:
        Replica& replica_;
    };
}