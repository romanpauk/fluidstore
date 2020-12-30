#pragma once

#include <type_traits>

namespace crdt
{
    template < typename Tag, typename T > struct tagged_type
        : public T
    {
        typedef Tag tag_type;
        typedef T   value_type;

        template < typename... Args > tagged_type(Args&&... args)
            : T(std::forward< Args >(args)...)
        {}

        template < typename TagT, typename std::enable_if_t< std::is_same_v< Tag, TagT >, bool > = true > T& get() { return *this; }
    };

    template < typename... Types > struct tagged_collection
        : public Types...
    {
        template < typename... Args > tagged_collection(Args&&... args)
            : Types(std::forward< Args >(args))...
        {}

        template < typename Tag > using type = std::remove_reference_t< decltype(std::declval< tagged_collection< Types... > >().get< Tag >()) >;

        using Types::get...;
    };
}