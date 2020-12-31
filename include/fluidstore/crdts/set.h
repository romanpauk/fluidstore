#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/default_hook.h>
#include <fluidstore/crdts/allocator.h>

namespace crdt
{
    template < typename Key, typename Allocator, typename Tag, typename Hook, typename Delta > class set_base
        : public Hook::template hook< Allocator, Delta, set_base< Key, Allocator, Tag, Hook, Delta > >
        , public dot_kernel< 
            Key, void, 
            typename allocator_traits< Allocator >::template allocator_type< Tag >, 
            set_base< Key, Allocator, Tag, Hook, Delta >, Tag 
        >
    {
        typedef set_base< Key, Allocator, Tag, Hook, Delta > set_base_type;
        
        typedef dot_kernel< 
            Key, void, 
            typename allocator_traits< Allocator >::template allocator_type< Tag >, 
            set_base_type, Tag 
        > dot_kernel_type;
        
        friend class dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        
        typedef typename Hook::template hook< Allocator, Delta, set_base_type > hook_type;
        typedef typename allocator_type::replica_type replica_type;

        template < typename AllocatorT, typename TagT, typename HookT > struct rebind { typedef set_base< Key, AllocatorT, TagT, HookT, Delta > type; };
        
        struct delta_extractor
        {
            template < typename Delta > void apply(set_base_type& instance, Delta& delta) {}
        };

        set_base(allocator_type allocator)
            : hook_type(allocator, allocator.get_replica().generate_instance_id())
            , dot_kernel_type(allocator_traits< Allocator >::get_allocator< Tag >(allocator))
        {}

        set_base(allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : hook_type(allocator, id)
            , dot_kernel_type(allocator_traits< Allocator >::get_allocator< Tag >(allocator))
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            auto& delta = mutable_delta();
            insert(delta, key);
            insert_context context;
            this->merge(delta, context);
            this->commit_delta(delta);
            return { context.result.first, context.result.second };
        }

        template < typename Delta > void insert(Delta& delta, const Key& key)
        {
            auto replica_id = this->get_allocator().get_replica().get_id();
            auto counter = this->counters_.get(replica_id) + 1;
            delta.counters_.emplace(replica_id, counter);
            delta.values_[key].dots.emplace(replica_id, counter);
        }
    };

    template < typename Key, typename Allocator, typename Tag, typename Hook > class set_base< Key, Allocator, Tag, Hook, void >
        : public dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag >
    {
        typedef dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, void >, Tag > dot_kernel_type;

    public:
        typedef Allocator allocator_type;

        set_base(allocator_type allocator)
            : dot_kernel_type(allocator)
        {}
    };

    template < typename Key, typename Allocator, typename Hook = default_hook, 
        typename Delta = set_base< 
            Key, 
            typename allocator_traits< Allocator >::template allocator_type< tag_delta >,
            tag_delta, default_hook, void 
        >
    > class set
        : public set_base< Key, Allocator, tag_state, Hook, Delta >
    {
        typedef set_base< Key, Allocator, tag_state, Hook, Delta > set_base_type;

    public:
        typedef Allocator allocator_type;

        set(allocator_type allocator)
            : set_base_type(allocator, allocator.get_replica().generate_instance_id())
        {}

        set(allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : set_base_type(allocator, id)
        {}
    };
}