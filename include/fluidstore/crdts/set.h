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
        typedef set_base< Key, Allocator, Tag, Hook, Delta > set_base_type;
        typedef dot_kernel< Key, void, Allocator, set_base_type, Tag > dot_kernel_type;
        
        friend dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        
        typedef typename Hook::template hook< Allocator, Delta, set_base_type > hook_type;
        typedef typename allocator_type::replica_type replica_type;

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
            auto replica_id = this->get_allocator().get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(delta.get_allocator(), replica_id, counter);
            delta.values_.emplace(delta.get_allocator(), delta.get_allocator(), key, nullptr).first->second.dots.emplace(delta.get_allocator(), replica_id, counter);
        }
    };

    template < typename Key, typename Allocator, typename Tag, typename Hook > class set_base< Key, Allocator, Tag, Hook, void >
        : public Hook::template hook< Allocator, void, set_base< Key, Allocator, Tag, Hook, void > >
        , public dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag >
    {
        typedef dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag > dot_kernel_type;
        typedef set_base< Key, Allocator, Tag, Hook, void > set_base_type;
        typedef typename Hook::template hook< Allocator, void, set_base_type > hook_type;

    public:
        typedef Allocator allocator_type;
        
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
        typedef set_base< Key, Allocator, tag_state, Hook, Delta > set_base_type;

    public:
        typedef Allocator allocator_type;
        typedef Hook hook_type;
        typedef Delta delta_type;

        set(allocator_type& allocator)
            : set_base_type(allocator)
        {}
    };
}