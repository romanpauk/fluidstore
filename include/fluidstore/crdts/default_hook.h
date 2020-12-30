#pragma once

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
                , delta_(allocator)
            {}

            allocator_type& get_allocator() { return allocator_; }
            const id_type& get_id() const { return id_; }
            void commit_delta() { this->delta_.reset(); }
            
            // protected:
            allocator_type allocator_;
            id_type id_;
            Delta delta_;
        };
    };
}