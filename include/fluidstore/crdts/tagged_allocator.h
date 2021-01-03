#pragma once

#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/allocator_traits.h>
#include <fluidstore/crdts/tagged_collection.h>

namespace crdt
{
    template < typename Tag, typename Allocator > class tagged_allocator_base
        : public Allocator
        , public tagged_type< Tag, tagged_allocator_base< Tag, Allocator > >
    {
    public:
        template < typename U > struct rebind
        {
            using other = tagged_allocator_base< Tag, typename Allocator::template rebind< U >::other >;
        };

        template < typename... Args > tagged_allocator_base(Args&&... args)
            : Allocator(std::forward< Args >(args)...)
        {}

        tagged_allocator_base(const tagged_allocator_base< Tag, Allocator >& allocator)
            : Allocator(allocator)
        {}
    };

    struct tag_state;
    struct tag_delta;

    template <
        typename Replica,
        typename T = unsigned char,
        typename StateAllocator = std::allocator< T >,
        typename DeltaAllocator = StateAllocator,
        typename Tag = tag_state,
        typename Container = allocator_container< void >
    > class tagged_allocator
        : public tagged_allocator_base< tag_state, StateAllocator >
        , public tagged_allocator_base< tag_delta, DeltaAllocator >
        , public ::crdt::allocator< Replica, T, StateAllocator, Container >
    {
        template < typename Replica, typename T, typename StateAllocator, typename DeltaAllocator, typename Tag, typename Container > friend class tagged_allocator;

        typedef typename tagged_collection<
            tagged_allocator_base< tag_state, StateAllocator >,
            tagged_allocator_base< tag_delta, DeltaAllocator >
        >::template type< Tag > allocator_type;

    public:
        using value_type = typename allocator_type::value_type;
        using allocator_type::allocate;
        using allocator_type::deallocate;

        using replica_type = Replica;

        template< typename U, typename ContainerU = Container, typename TagU = Tag > struct rebind
        {
            using other = tagged_allocator< Replica, U,
                typename StateAllocator::template rebind< U >::other,
                typename DeltaAllocator::template rebind< U >::other,
                TagU,
                ContainerU
            >;
        };

        tagged_allocator(Replica& replica)
            : ::crdt::allocator< Replica, T, Container >(replica)
        {}

        /*
        tagged_allocator(Replica& replica, const StateAllocator& state)
            : replica_(&replica)
            , tagged_allocator_base< tag_state, StateAllocator >(state)
            , tagged_allocator_base< tag_delta, DeltaAllocator >(DeltaAllocator())
        {}

        tagged_allocator(Replica& replica, const DeltaAllocator& delta)
            : replica_(&replica)
            , tagged_allocator_base< tag_state, StateAllocator >(StateAllocator())
            , tagged_allocator_base< tag_delta, DeltaAllocator >(delta)
        {}
        */

        tagged_allocator(Replica& replica, const StateAllocator& state, const DeltaAllocator& delta)
            : ::crdt::allocator< Replica, T, StateAllocator, Container >(replica)
            , tagged_allocator_base< tag_state, StateAllocator >(state)
            , tagged_allocator_base< tag_delta, DeltaAllocator >(delta)
        {}

        template < typename ReplicaU, typename U, typename StateAllocatorU, typename DeltaAllocatorU, typename TagU, typename ContainerU > tagged_allocator(
            const tagged_allocator< ReplicaU, U, StateAllocatorU, DeltaAllocatorU, TagU, ContainerU >& other
        )
            : tagged_allocator_base< tag_state, StateAllocator >(static_cast<const tagged_allocator_base< tag_state, StateAllocatorU >&>(other))
            , tagged_allocator_base< tag_delta, DeltaAllocator >(static_cast<const tagged_allocator_base< tag_delta, DeltaAllocatorU >&>(other))
            , ::crdt::allocator< Replica, T, StateAllocator, Container >(other)
        {}

        template < typename ReplicaU, typename U, typename StateAllocatorU, typename DeltaAllocatorU, typename TagU, typename ContainerU > tagged_allocator(
            const tagged_allocator< ReplicaU, U, StateAllocatorU, DeltaAllocatorU, TagU, ContainerU >& other, const Container& container
        )
            : tagged_allocator_base< tag_state, StateAllocator >(static_cast<const tagged_allocator_base< tag_state, StateAllocatorU >&>(other))
            , tagged_allocator_base< tag_delta, DeltaAllocator >(static_cast<const tagged_allocator_base< tag_delta, DeltaAllocatorU >&>(other))
            , ::crdt::allocator< Replica, T, StateAllocator, Container >(other, container)
        {}
    };

    template < typename Replica, typename T, typename StateAllocator, typename DeltaAllocator, typename Tag, typename Container > class allocator_traits<
        tagged_allocator< Replica, T, StateAllocator, DeltaAllocator, Tag, Container >
    >
    {
    public:
        template < typename TagT > using allocator_type = typename tagged_allocator< Replica, T, StateAllocator, DeltaAllocator, TagT, Container >::template rebind< T, Container, TagT >::other;
        template < typename TagT, typename Allocator > static allocator_type< TagT > get_allocator(Allocator& allocator) { return allocator; }
    };

#if 0
    template < typename Tag, typename Allocator > class tagged_allocator
        : public Allocator
        , public tagged_type< Tag, tagged_allocator< Tag, Allocator > >
    {
    public:
        typedef Tag tag_type;

        template < typename... Args > tagged_allocator(Args&&... args)
            : Allocator(std::forward< Args >(args)...)
        {}

        tagged_allocator(const tagged_allocator< Tag, Allocator >& allocator)
            : Allocator(allocator)
        {}

        // using value_type = typename Allocator::value_type;

        template < typename U > struct rebind
        {
            using other = tagged_allocator< Tag, typename Allocator::template rebind< U >::other >;
        };
    };

    template <
        typename Replica,
        typename T,
        typename Tag,
        typename... Allocators
    > class tagged_allocators
        : 
        //public tagged_collection< typename Allocators::template rebind< T >::other... >::template type< Tag >,
        public tagged_collection< typename Allocators::template rebind< T >::other... >
    {
        template < typename Replica, typename U, typename TagU, typename... AllocatorsU > friend class tagged_allocators;

    public:
        using allocator_type = typename tagged_collection< typename Allocators::template rebind< T >::other... >::template type< Tag >;
        using value_type = typename allocator_type::value_type;
        using allocator_type::allocate;
        using allocator_type::deallocate;

        template< typename U > struct rebind
        {
            using other = tagged_allocators< Replica, U, Tag, typename Allocators::template rebind< T >::other... >;
        };

        typedef Replica replica_type;

        template < typename... Args > tagged_allocators(Replica& replica, Args&&... args)
            : //tagged_collection< typename Allocators::template rebind< T >::other... >::template type< Tag >(replica)
            tagged_collection< typename Allocators::template rebind< T >::other... >(args...)
            , replica_(&replica)
            //, allocators_(std::forward< Args >(args)...)
        {}

        template < typename U, typename... AllocatorsT > tagged_allocators(const tagged_allocators< Replica, U, Tag, AllocatorsT... >& other)
            : //tagged_collection< typename AllocatorsT::template rebind< T >::other... >::template type< Tag >(*other.replica_)
            tagged_collection< typename AllocatorsT::template rebind< T >::other... >(other)
            , replica_(other.replica_)
        {}

        auto& get_replica() const { return *replica_; }

        /*
        value_type* allocate(std::size_t n)
        {
            return allocators_.get< Tag >().allocate(n);
        }

        void deallocate(value_type* p, std::size_t n) noexcept
        {
            allocators_.get< Tag >().deallocate(p, n);
        }
        */

    private:
        Replica* replica_;
        //tagged_collection< typename Allocators::template rebind< T >::other... > allocators_;
    };

    /*
    template < typename Allocator > struct allocator_traits
    {
        template < typename Tag > static Allocator& get_allocator(Allocator& allocator) { return allocator; }
        template < typename Tag > using allocator_type = Allocator;
    };
    */

    /*
    template < typename Replica, typename... Allocators > struct allocator_traits< tagged_allocator< Replica, Allocators... > >
    {
        template < typename Tag > static auto& get_allocator(tagged_allocator< Replica, Allocators...>& allocator) { return allocator.get< Tag >(); }
        template < typename Tag > using allocator_type = typename tagged_allocator< Replica, Allocators... >::template type< Tag >;
    };
    */
#endif
}