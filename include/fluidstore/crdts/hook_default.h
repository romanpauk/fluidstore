#pragma once

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_default
    {
        hook_default(Allocator& allocator)
            : allocator_(allocator)
        {}

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