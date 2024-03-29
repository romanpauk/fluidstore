#pragma once

#include <fluidstore/crdt/detail/allocator_ptr.h>

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_extract
    {
        template < typename AllocatorT > hook_extract(AllocatorT&& allocator)
            : allocator_(std::forward< AllocatorT >(allocator))
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
            allocator_ = std::move(other.allocator_);
            delta_ = std::move(other.delta_);
            return *this;
        }

        template < typename DeltaT > void commit_delta(DeltaT&& delta)
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
            // Delta delta(std::move(delta_));

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

        Allocator& get_allocator()
        {
            return allocator_;
        }

    private:
        Allocator allocator_;
        allocator_ptr_base< Delta > delta_;
    };
}