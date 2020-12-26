#pragma once

#include <fluidstore/crdts/replica.h>

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

        template< typename R, typename U > struct rebind_replica { typedef allocator< R, U >::type; };

        template < typename Instance > struct hook 
            : public replica_type::template hook< Instance >
        {
            template< typename T > hook(T&& args)
                : replica_type::template hook< Instance >(args)
            {}
        };
     
        template < typename... Args > allocator(Replica& replica, Args&&... args)
            : Allocator(std::forward< Args >(args)...)
            , replica_(&replica)
        {}

        template < typename U, typename AllocatorU > allocator(const allocator< Replica, U, AllocatorU >& other)
            : Allocator(other)
            , replica_(other.replica_)
        {}

        allocator< Replica, T, Allocator >& operator = (const allocator< Replica, T, Allocator >& other)
        {
            replica_ = other.replica_;
            return *this;
        }

        auto& get_replica() const { return *replica_; }

        template < typename Instance, typename DeltaInstance > void merge(const Instance& target, const DeltaInstance& source)
        {
            this->replica_->merge(target, source);
        }

    private:
        Replica* replica_;
    };
}