#pragma once

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
}