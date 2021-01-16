#pragma once

#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/allocator_ptr.h>

#include <memory>

namespace crdt
{
    struct default_delta_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator allocator, const id_type& id)
                : allocator_(allocator)
                , id_(id)
            {}

            hook(hook< Allocator, Delta, Instance >&& other)
                : allocator_(other.allocator_) // std::move(other.allocator_))
                , id_(std::move(other.id_))
            {}

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }

        private:
            allocator_type allocator_;
            id_type id_;
        };
    };

    struct default_state_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator allocator, const id_type& id)
                : allocator_(allocator)
                , id_(id)
            {}

            hook(hook< Allocator, Delta, Instance >&& other)
                : allocator_(other.allocator_)
                , id_(std::move(other.id_))
            {}

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }
            
            auto mutable_delta() 
            { 
                return Delta(allocator_);
            }

            void commit_delta(Delta&) 
            { 
                allocator_.update();
            }
            
        private:
            allocator_type allocator_;
            id_type id_;
        };
    };
}