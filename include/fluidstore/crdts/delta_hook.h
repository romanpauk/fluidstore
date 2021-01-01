#pragma once

#include <fluidstore/crdts/default_hook.h>

#include <optional>

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
            {}

            ~hook()
            {
                delta_persistent_.reset(get_allocator());
            }

            void commit_delta(Delta& delta)
            {
                if (!delta_persistent_)
                {
                    delta_persistent_.emplace(get_allocator(), get_allocator());
                }

                delta_persistent_->merge(delta);
                default_hook::template hook< Allocator, Delta, Instance >::commit_delta(delta);
            }

            Delta extract_delta()
            {
                Delta delta(get_allocator());
                if (delta_persistent_)
                {
                    delta.merge(*delta_persistent_);

                    typename Instance::delta_extractor extractor;
                    extractor.apply(*static_cast<Instance*>(this), delta);

                    delta_persistent_.reset(get_allocator());
                }
                else
                {
                    // TODO: investigate how this can happen
                }

                return delta;
            }

        private:
            allocator_ptr_base< Delta > delta_persistent_;
        };
    };
}