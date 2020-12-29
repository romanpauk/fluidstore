#pragma once

#include <fluidstore/crdts/set.h>

namespace crdt
{
    template < typename DeltaInstance > class delta_hook
    {
    public:
        template < typename Allocator, typename Instance > struct hook
            : public default_hook::template hook< Allocator, Instance >
            , public Allocator::replica_type::template hook< Allocator, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(allocator_type allocator, const id_type& id)
                : default_hook::template hook< allocator_type, Instance >(allocator, id)
                , allocator_type::replica_type::template hook< allocator_type, Instance >(allocator, id)
                , delta_(
                    std::allocator_traits< Allocator >::rebind_alloc< typename DeltaInstance::allocator_type >(
                        static_cast<Instance*>(this)->get_allocator()
                    )
                    , id
                )
            {}

            DeltaInstance extract_delta()
            {
                DeltaInstance delta(delta_.get_allocator(), delta_.get_id());
                std::swap(delta, delta_);
                return delta;
            }

            template < typename InstanceT, typename DeltaInstanceT > void merge_hook(const InstanceT& target, const DeltaInstanceT& source)
            {
                this->get_allocator().get_replica().merge(target, source);
            }

        private:
            DeltaInstance delta_;
        };
    };

    class delta_replica_hook
    {
    public:
        template < typename Allocator, typename Instance > struct hook
            : public default_hook::template hook< Allocator, Instance >
            , public Allocator::replica_type::template hook< Allocator, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(allocator_type allocator, const id_type& id)
                : default_hook::template hook< Allocator, Instance >(allocator, id)
                , allocator_type::replica_type::template hook< Allocator, Instance >(allocator, id)
            {}

            using allocator_type::replica_type::template hook< Allocator, Instance >::merge_hook;
        };
    };
}