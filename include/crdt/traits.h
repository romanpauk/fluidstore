#pragma once

#include <sqlstl/allocator.h>

namespace crdt
{
    struct memory {};
    struct sqlite {};
    template < typename Tag > struct traits;

    template <> struct traits< memory >
    {
        template < typename T, typename Factory > using Set = std::set< T >;
        template < typename K, typename V, typename Factory > using Map = std::map< K, V >;
        using Allocator = allocator;
    };

    template <> struct traits< sqlite >
    {
        template < typename T, typename Factory > using Set = sqlstl::set< T, Factory >;
        template < typename K, typename V, typename Factory > using Map = sqlstl::map< K, V, Factory >;
        using Allocator = sqlstl::allocator;
    };
}
