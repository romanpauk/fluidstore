#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/default_hook.h>

namespace crdt
{
    template < typename Key, typename Allocator, typename Tag, typename Hook, typename Delta > class set_base
        : public Hook::template hook< Allocator, Delta, set_base< Key, Allocator, Tag, Hook, Delta > >
        , public dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, Delta >, Tag 
        >
    {
        using set_base_type = set_base< Key, Allocator, Tag, Hook, Delta >;
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set_base_type, Tag >;
        
        friend dot_kernel_type;

    public:
        using allocator_type = Allocator;
        using hook_type = typename Hook::template hook< Allocator, Delta, set_base_type >;
                
        template < typename AllocatorT, typename HookT = Hook, typename TagT = Tag > struct rebind
        {
            using other = set_base< Key, AllocatorT, TagT, HookT, Delta >;
        };
        
        struct delta_extractor
        {
            template < typename Delta > void apply(set_base_type& instance, Delta& delta) {}
        };

        set_base(allocator_type& allocator)
            : hook_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            
            auto delta = mutable_delta(deltaallocator);
            insert(delta, key);
            insert_context context;
            merge(delta, context);
            commit_delta(delta);

            return { context.result.first, context.result.second };
        }

        template < typename Delta > void insert(Delta& delta, const Key& key)
        {
            auto dot = this->get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);
        }
    };

    template < typename Key, typename Allocator, typename Tag, typename Hook > class set_base< Key, Allocator, Tag, Hook, void >
        : public Hook::template hook< Allocator, void, set_base< Key, Allocator, Tag, Hook, void > >
        , public dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag >
    {
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag >;
        using set_base_type = set_base< Key, Allocator, Tag, Hook, void >;
        using hook_type = typename Hook::template hook< Allocator, void, set_base_type >;

    public:
        using allocator_type = Allocator;
        
        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = set_base< Key, AllocatorT, Tag, HookT, void >;
        };
        
        set_base(allocator_type& allocator)
            : hook_type(allocator)
        {}

        set_base(set_base_type&& other) = default;
    };

    template < typename Key, typename Allocator, typename Hook = default_state_hook, 
        typename Delta = set_base< Key, Allocator, tag_delta, default_delta_hook, void >
    > class set
        : public set_base< Key, Allocator, tag_state, Hook, Delta >
    {
        using set_base_type = set_base< Key, Allocator, tag_state, Hook, Delta >;

    public:
        using allocator_type = Allocator;
        using hook_type = Hook;
        using delta_type = Delta;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = set< Key, AllocatorT, HookT, Delta >;
        };

        set(allocator_type& allocator)
            : set_base_type(allocator)
        {}
    };
}