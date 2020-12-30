#pragma once

#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/tagged_collection.h>

#include <memory>

namespace crdt
{
    template < typename Replica = replica<>, typename T = unsigned char, typename Allocator = std::allocator< T > > class allocator
        : public Allocator
    {
    public:
        template < typename Replica, typename U, typename AllocatorU > friend class allocator;
        typedef Replica replica_type;

        template< typename U > struct rebind
        {
            typedef allocator< Replica, U,
                typename std::allocator_traits< Allocator >::template rebind_alloc< U >
            > other;
        };

        template < typename... Args > allocator(Replica& replica, Args&&... args)
            : Allocator(std::forward< Args >(args)...)
            , replica_(&replica)
        {}

        template < typename ReplicaU, typename U, typename AllocatorU > allocator(const allocator< ReplicaU, U, AllocatorU >& other)
            : Allocator(other)
            , replica_(other.replica_)
        {}

        allocator< Replica, T, Allocator >& operator = (const allocator< Replica, T, Allocator >& other)
        {
            replica_ = other.replica_;
            return *this;
        }

        auto& get_replica() const { return *replica_; }

    private:
        Replica* replica_;
    };

    template < typename Replica = replica<>, typename... Allocators > class tagged_allocator
        : public tagged_collection< Allocators... >
    {
    public:
        typedef Replica replica_type;

        template< typename... Args > tagged_allocator(Replica& replica, Args&&... args)
            : tagged_collection< Allocators... >(std::forward< Args >(args)...)
            , replica_(&replica)
        {}

        auto& get_replica() const { return *replica_; }

    private:
        Replica* replica_;
    };

    template < typename Allocator > struct allocator_traits 
    {
        template < typename Tag > static Allocator& get_allocator(Allocator& allocator) { return allocator; }
        template < typename Tag > using allocator_type = Allocator;
    };

    template < typename Replica, typename... Allocators > struct allocator_traits< tagged_allocator< Replica, Allocators... > >
    {
        template < typename Tag > static auto& get_allocator(tagged_allocator< Replica, Allocators...>& allocator) { return allocator.get< Tag >(); }
        template < typename Tag > using allocator_type = typename tagged_allocator< Replica, Allocators... >::template type< Tag >;
    };
}