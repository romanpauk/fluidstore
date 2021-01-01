#pragma once

namespace crdt
{
    template < typename Allocator > class allocator_traits
    {
    public:
        template < typename Tag > static Allocator& get_allocator(Allocator& allocator) { return allocator; }
        template < typename Tag > using allocator_type = Allocator;
    };
}