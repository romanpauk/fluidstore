#pragma once

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_default
    {
        hook_default(Allocator& allocator)
            : allocator_(allocator)
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

        typename Allocator& get_allocator()
        {
            return allocator_;
        }

    private:
        Allocator allocator_;
    };
        
    template <> struct hook_default< void, void, void > {};
}