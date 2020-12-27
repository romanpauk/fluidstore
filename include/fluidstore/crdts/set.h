#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/allocators/arena_allocator.h>

namespace crdt
{
    template < typename Allocator > struct default_hook
    {
        typedef Allocator allocator_type;
        typedef typename allocator_type::replica_type::id_type id_type;

        template < typename Instance > struct hook
        {
            hook(Allocator alloc, const id_type& id)
                : allocator_(alloc)
                , id_(id)
            {}

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }

            template < typename Instance, typename DeltaInstance > void merge_hook(const Instance& target, const DeltaInstance& source) {}

        private:
            allocator_type allocator_;
            id_type id_;
        };
    };

    template < typename Key, typename Allocator, typename Tag = tag_state, typename Hook = default_hook< Allocator > > class set
        : public Hook::template hook< set< Key, Allocator, Tag, Hook > >
        , public dot_kernel< Key, void, Allocator, set< Key, Allocator, Tag, Hook >, Tag >
    {
        typedef set< Key, Allocator, Tag, Hook > set_type;
        typedef dot_kernel< Key, void, Allocator, set_type, Tag > dot_kernel_type;

    public:
        typedef Allocator allocator_type;
        typedef typename Hook::template hook< set_type > hook_type;
        typedef typename allocator_type::replica_type replica_type;

        template < typename AllocatorT, typename TagT > struct rebind { typedef set< Key, AllocatorT, TagT, Hook > type; };

        set(Allocator allocator)
            : hook_type(allocator, allocator.get_replica().generate_instance_id())
            , dot_kernel_type(allocator) 
        {}

        set(Allocator allocator, typename Allocator::replica_type::id_type id)
            : hook_type(allocator, id)
            , dot_kernel_type(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            arena< 1024 > buffer;
            arena_allocator< void, allocator< typename replica_type::delta_replica_type > > allocator(buffer, this->get_allocator().get_replica());
            typename rebind< decltype(allocator), tag_delta >::type delta(allocator, this->get_id());
            // set_type delta(this->allocator_, this->get_id());

            insert(delta, key);
         
            insert_context context;
            this->merge(delta, context);
            this->merge_hook(*this, delta);
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
}