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
        template < typename T > struct Allocator : std::allocator< T > 
        {
            template < typename Alloc > Allocator(Alloc&& alloc) {}
            template < typename Alloc, typename Value > Allocator(Alloc&& alloc, const Value&, bool replace = false) {}

            std::string get_name() { return ""; }
        };
        
        template < typename T > using Set = std::set< T, std::less< T >, std::scoped_allocator_adaptor< Allocator< T > > >;
        template < typename K, typename V > using Map = std::map< K, V, std::less< K >, std::scoped_allocator_adaptor< Allocator< std::pair< const K, V > > > >;
        template < typename Allocator, typename... Args > using Tuple = std::tuple< Args... >;

        template < typename T > struct type_traits
        {
            static const bool embeddable = true;
        };

        template < typename T > using TypeTraits = type_traits< T >;
    };

    template <> struct traits< sqlite >
    {
        template < typename T > using Allocator = sqlstl::allocator< T >;
        template < typename T > using Set = sqlstl::set< T, Allocator< T > >;
        template < typename K, typename V > using Map = sqlstl::map< K, V, Allocator< void > >;
        template < typename... Args > using Tuple = sqlstl::tuple< Allocator< void >, Args... >;

        template < typename T > using TypeTraits = sqlstl::type_traits< T >;
    };
}
