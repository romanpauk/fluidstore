#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/allocators/arena_allocator.h>

namespace crdt
{
    struct default_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator alloc, const id_type& id)
                : allocator_(alloc)
                , id_(id)
                , delta_(alloc)
            {}

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }

            void merge_hook() 
            {
                this->delta_.reset();
            }

        // protected:
            allocator_type allocator_;
            id_type id_;
            Delta delta_;
        };
    };

    struct extract_delta_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
            : public default_hook::template hook< Allocator, Delta, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator alloc, const id_type& id)
                : default_hook::template hook< Allocator, Delta, Instance >(alloc, id)
                , delta_persistent_(alloc)
            {}

            void merge_hook()
            {
                this->delta_persistent_.merge(static_cast<Instance*>(this)->delta_);
                static_cast<Instance*>(this)->delta_.reset();
            }

            Delta extract_delta()
            {
                Delta dump(get_allocator());
                dump.merge(delta_persistent_);

                typename Instance::delta_extractor extractor;
                extractor.apply(*static_cast<Instance*>(this), dump);

                delta_persistent_.reset();
                return dump;
            }

        protected:
            Delta delta_persistent_;
        };
    };

    template < typename Key, typename Allocator, typename Tag, typename Hook, typename Delta > class set_base
        : public Hook::template hook< Allocator, Delta, set_base< Key, Allocator, Tag, Hook, Delta > >
        , public dot_kernel< Key, void, Allocator, set_base< Key, Allocator, Tag, Hook, Delta >, Tag >
    {
        typedef set_base< Key, Allocator, Tag, Hook, Delta > set_base_type;
        typedef dot_kernel< Key, void, Allocator, set_base_type, Tag > dot_kernel_type;
        friend class dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        typedef typename Hook::template hook< allocator_type, Delta, set_base_type > hook_type;
        typedef typename allocator_type::replica_type replica_type;

        template < typename AllocatorT, typename TagT, typename HookT > struct rebind { typedef set_base< Key, AllocatorT, TagT, HookT, Delta > type; };
        
        struct delta_extractor
        {
            template < typename Delta > void apply(set_base_type& instance, Delta& delta) {}
        };

        set_base(allocator_type allocator)
            : hook_type(allocator, allocator.get_replica().generate_instance_id())
            , dot_kernel_type(allocator)
        {}

        set_base(allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : hook_type(allocator, id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            insert(delta_, key);
            insert_context context;
            this->merge(delta_, context);
            this->merge_hook();
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

    template < typename Key, typename Allocator, typename Hook = default_hook, typename Delta = set_base< Key, Allocator, tag_delta, default_hook, void > 
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