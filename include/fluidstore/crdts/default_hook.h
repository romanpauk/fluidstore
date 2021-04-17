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
            
            hook(Allocator& allocator)
                : allocator_(allocator)
            {}

            hook(hook< Allocator, Delta, Instance >&& other) = default;

            allocator_type& get_allocator() { return allocator_; }

        private:
            allocator_type allocator_;
        };
    };

    struct default_state_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
        {
            typedef Allocator allocator_type;
            
            hook(Allocator& allocator)
                : allocator_(allocator)
            {}

            hook(hook< Allocator, Delta, Instance >&& other) = default;
            
            allocator_type& get_allocator() { return allocator_; }
            
            template < typename Allocator > auto mutable_delta(Allocator& allocator)
            {
                return typename Delta::template rebind< Allocator >::other(allocator);
            }

            template < typename DeltaT > void commit_delta(DeltaT&) 
            { 
                allocator_.update();
            }
            
        private:
            allocator_type allocator_;
        };
    };
}