#pragma once

namespace crdt
{
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
}