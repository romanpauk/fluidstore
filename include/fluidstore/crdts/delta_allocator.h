#pragma once

#include <fluidstore/crdts/set.h>

namespace crdt
{
    template < typename Allocator, typename DeltaInstance > class delta_hook
    {
    public:
        typedef Allocator allocator_type;
        typedef typename allocator_type::replica_type::id_type id_type;

        template < typename Instance > struct hook
            : public default_hook< Allocator >::template hook< Instance >
        {
            hook(allocator_type allocator, const id_type& id)
                : default_hook< allocator_type >::template hook< Instance >(allocator, id)
                , delta_(static_cast<Instance*>(this)->get_allocator(), id)
            {}

            DeltaInstance extract_delta()
            {
                DeltaInstance delta(static_cast<Instance*>(this)->get_allocator(), static_cast<Instance*>(this)->get_id());
                std::swap(delta, delta_);
                return delta;
            }

            template < typename Instance, typename DeltaInstance > void merge_hook(const Instance& target, const DeltaInstance& source)
            {
                this->delta_.merge(source);
            }
        private:
            DeltaInstance delta_;
        };
    };

    template < typename Allocator > class delta_replica_hook
    {
    public:
        typedef Allocator allocator_type;
        typedef typename allocator_type::replica_type::id_type id_type;

        template < typename Instance > struct hook
            : public default_hook< Allocator >::template hook< Instance >
            , public allocator_type::replica_type::template hook< Instance >
        {
            hook(allocator_type allocator, const id_type& id)
                : default_hook< allocator_type >::template hook< Instance >(allocator, id)
                , allocator_type::replica_type::template hook< Instance >(allocator, id)
            {}

            using allocator_type::replica_type::template hook< Instance >::merge_hook;
        };
    };
}