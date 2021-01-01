#pragma once

#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/allocator_ptr.h>

#include <memory>

namespace crdt
{
    struct default_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator allocator, const id_type& id)
                : allocator_(allocator)
                , id_(id)
            {}

            ~hook()
            {
                delta_.reset(allocator_);
            }

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }
            
            auto& mutable_delta() 
            { 
                if (!delta_)
                {
                    delta_.emplace(allocator_, allocator_);
                }
                
                return *delta_;
            }

            void commit_delta(Delta&) 
            { 
                delta_.reset(allocator_); 
            }
            
        private:
            allocator_type allocator_;
            id_type id_;

            allocator_ptr_base< Delta > delta_;
        };
    };
}