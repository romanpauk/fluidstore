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

            hook(Allocator alloc, const id_type& id)
                : default_hook::template hook< Allocator, Delta, Instance >(alloc, id)
                , delta_persistent_(alloc)
            {}

            void merge_hook()
            {
                this->delta_persistent_.merge(static_cast<Instance*>(this)->delta_);
                static_cast<Instance*>(this)->delta_.reset();
            }

            Delta extract_delta()
            {
                Delta dump(get_allocator());
                dump.merge(delta_persistent_);

                typename Instance::delta_extractor extractor;
                extractor.apply(*static_cast<Instance*>(this), dump);

                delta_persistent_.reset();
                return dump;
            }

        protected:
            Delta delta_persistent_;
        };
    };
}