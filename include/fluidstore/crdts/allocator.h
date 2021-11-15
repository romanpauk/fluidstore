#pragma once

#include <fluidstore/crdts/replica.h>

#include <memory>

namespace crdt
{
    template < typename T > struct allocator_container 
    {
        typedef T container_type;

        allocator_container(T* container)
            : container_(container)
        {}

        allocator_container(const allocator_container< T >& other)
            : container_(other.container_)
        {}

        T& get_container() { return *container_; }
        const T& get_container() const { return *container_; }

        void update() { container_->update(); }

        void set_container(T* container) { container_ = container; }

    private:
        T* container_;
    };

    template<> struct allocator_container<void> 
    {
        allocator_container(void* = 0) {}
        template< typename T > allocator_container(const allocator_container<T>&) {}
        void update() {}
        void set_container(void*) {}
    };

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

        template < typename Al > allocator(Replica& replica, Al&& al)
            : Allocator(std::forward<Al>(al))
            , replica_(replica)
        {}

        allocator(const allocator< Replica, T, Allocator, Container >& other)
            : Allocator(other)
            , Container(other)
            , replica_(other.replica_)
        {}

        template < typename ReplicaU, typename U, typename AllocatorU, typename ContainerU > allocator(
            const allocator< ReplicaU, U, AllocatorU, ContainerU >& other
        )
            : Allocator(other)
            , Container(other)
            , replica_(other.replica_)
        {}
        
        template < typename ReplicaU, typename U, typename AllocatorU, typename ContainerU > allocator(
            const allocator< ReplicaU, U, AllocatorU, ContainerU >& other, const Container& container
        )
            : Allocator(other)
            , Container(container)
            , replica_(other.replica_)
        {}
        
        auto& get_replica() const { return replica_; }

    private:
        Replica& replica_;
    };
}