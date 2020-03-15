#pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/set.h>
#include <sqlstl/map.h>
#include <sqlstl/tuple.h>

#include <set>
#include <map>
#include <tuple>

namespace crdt
{
    struct memory {};
    struct sqlite {};
    template < typename Tag > struct traits;

    template <> struct traits< memory >
    {
        template < typename T, typename Allocator > using Set = std::set< T >;
        template < typename K, typename V, typename Allocator > using Map = std::map< K, V >;
        template < typename Allocator, typename... Args > using Tuple = std::tuple< Args... >;
        using Allocator = allocator;
    };

    template <> struct traits< sqlite >
    {
        template < typename T, typename Allocator > using Set = sqlstl::set< T, Allocator >;
        template < typename K, typename V, typename Allocator > using Map = sqlstl::map< K, V, Allocator >;
        template < typename Allocator, typename... Args > using Tuple = sqlstl::tuple< Allocator, Args... >;
        using Allocator = sqlstl::allocator;
    };
}
