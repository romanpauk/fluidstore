#pragma once

#include <fluidstore/crdts/default_hook.h>

namespace crdt
{
    struct delta_hook
    {
        template < typename Allocator, typename Delta, typename Instance > struct hook
            : public default_hook::template hook< Allocator, Delta, Instance >
        {
            typedef Allocator allocator_type;
            typedef typename allocator_type::replica_type::id_type id_type;

            hook(Allocator allocator, const id_type& id)
                : default_hook::template hook< Allocator, Delta, Instance >(allocator, id)
                , delta_persistent_(allocator)
            {}

            void commit_delta(Delta& delta)
            {
                //if (!delta_persistent_)
                //{
                //    delta_persistent_.emplace(this->get_allocator());
                //}

                this->delta_persistent_.merge(delta);
                default_hook::template hook< Allocator, Delta, Instance >::commit_delta(delta);
            }

            Delta extract_delta()
            {
                Delta delta(get_allocator());
                delta.merge(delta_persistent_);

                typename Instance::delta_extractor extractor;
                extractor.apply(*static_cast<Instance*>(this), delta);

                delta_persistent_.reset();
                return delta;
            }

        protected:
            //std::optional< Delta > delta_persistent_;
            Delta delta_persistent_;
        };
    };
}