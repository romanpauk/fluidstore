#pragma once

#include <fluidstore/crdts/allocator_ptr.h>

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_extract
    {
        hook_extract(Allocator& allocator)
            : allocator_(allocator)
        {}

        ~hook_extract()
        {
            delta_.reset(allocator_);
        }

        hook_extract(hook_extract< Container, Allocator, Delta >&&) = default;

        hook_extract< Container, Allocator, Delta >& operator = (hook_extract< Container, Allocator, Delta >&& other)
        {
            assert(!delta_);

            // TODO: not everytime moving the allocator makes sense
            // allocator_ = std::move(other.allocator_);
            delta_ = std::move(other.delta_);
            return *this;
        }

        template < typename Delta > void commit_delta(Delta&& delta)
        {
            if (!delta_)
            {
                delta_.emplace(allocator_, allocator_);
            }

            delta_->merge(delta);
            allocator_.update();
        }

        Delta extract_delta()
        {
            // TODO: use move here
            //Delta delta(std::move(delta_));

            Delta delta(allocator_);
            if (delta_)
            {
                delta.merge(*delta_);

                typename Container::delta_extractor extractor;
                extractor.apply(*static_cast<Container*>(this), delta);

                delta_.reset(allocator_);
            }

            return delta;
        }

        typename Allocator& get_allocator()
        {
            return allocator_;
        }

    private:
        Allocator allocator_;
        allocator_ptr_base< Delta > delta_;
    };
}