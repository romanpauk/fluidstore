 #pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/set.h>
#include <sqlstl/map.h>
#include <sqlstl/tuple.h>

#include <set>
#include <map>
#include <tuple>
#include <scoped_allocator>

namespace crdt
{
    struct memory {};
    struct sqlite {};
    template < typename Tag > struct traits;

    template <> struct traits< memory >
    {
        template < typename T > struct Allocator : std::scoped_allocator_adaptor< std::allocator< T > >
        {
            template < typename Alloc > Allocator(Alloc&& alloc): std::scoped_allocator_adaptor< std::allocator< T > >(std::forward< Alloc >(alloc)) {}
            template < typename Alloc > Allocator(Alloc&& alloc, const std::string&): std::scoped_allocator_adaptor< std::allocator< T > >(std::forward< Alloc >(alloc)) {}
        };

        template < typename T > using Set = std::set< T, std::less< T >, Allocator< T > >;
        template < typename K, typename V > using Map = std::map< K, V, std::less< K >, Allocator< std::pair< const K, V > > >;
        template < typename Allocator, typename... Args > using Tuple = std::tuple< Args... >;
    };

    template <> struct traits< sqlite >
    {
        template < typename T > using Allocator = sqlstl::allocator< T >;
        template < typename T > using Set = sqlstl::set< T, Allocator< T > >;
        template < typename K, typename V > using Map = sqlstl::map< K, V, Allocator< void > >;
        template < typename... Args > using Tuple = sqlstl::tuple< Allocator< void >, Args... >;
    };
}
