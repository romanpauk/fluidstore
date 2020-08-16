#pragma once

#include <string>

namespace crdt
{
    template < typename T > class allocator
        : public std::allocator< T >
    {
    public:
        typedef T value_type;

        allocator() {}
        template < typename Allocator > allocator(Allocator&&) {}
        template < typename Allocator > allocator(Allocator&&, const std::string&) {}
    };

    class delta_crdt_base
    {
    public:
        virtual ~delta_crdt_base() {}

        virtual void apply() = 0;
        virtual void commit() = 0;
    };
}