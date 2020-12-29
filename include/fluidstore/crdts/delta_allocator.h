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
                , delta_replica_(
                    static_cast<Instance*>(this)->get_allocator().get_replica().get_id(),
                    static_cast<Instance*>(this)->get_allocator().get_replica().get_instance_id_sequence(), 
                    allocator
                )
                , delta_replica_allocator_(delta_replica_)
                , delta_(delta_replica_allocator_, id)
            {}

            auto extract_delta()
            {
                typedef typename DeltaInstance::template rebind< allocator_type, tag_delta, default_hook >::type delta_type;
                
                // TODO: support std::swap()
                delta_type delta(static_cast<Instance*>(this)->get_allocator(), delta_.get_id());
                delta.merge(delta_);
                delta_.reset();
                return delta;
            }

            template < typename InstanceT, typename DeltaInstanceT > void merge_hook(const InstanceT& target, const DeltaInstanceT& source)
            {
                this->get_allocator().get_replica().merge(target, source);
                this->get_allocator().get_replica().merge_with_replica(delta_replica_);
                this->get_allocator().get_replica().clear();
            }

        private:
            typename Allocator::replica_type delta_replica_;
            Allocator delta_replica_allocator_;
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