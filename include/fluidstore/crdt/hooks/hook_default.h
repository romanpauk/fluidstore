#pragma once

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_default
    {
        template < typename AllocatorT > hook_default(AllocatorT&& allocator)
            : allocator_(std::forward< AllocatorT >(allocator))
        {}

        hook_default(hook_default< Container, Allocator, Delta >&&) = default;

        hook_default< Container, Allocator, Delta >& operator = (hook_default< Container, Allocator, Delta > && other)
        {
            // TODO: not everytime moving the allocator makes sense
            allocator_ = std::move(other.allocator_);
            return *this;
        }

        template < typename DeltaT > void commit_delta(DeltaT&& delta)
        {}

        Allocator& get_allocator()
        {
            return allocator_;
        }

    private:
        Allocator allocator_;
    };
        
    template <> struct hook_default< void, void, void > {};
}