#pragma once

#include <type_traits>

namespace crdt
{
    template < typename Tag, typename T > struct tagged_type
    {
    // private:
        template < typename... Types > friend struct tagged_collection;
        template < typename TagT, typename std::enable_if_t< std::is_same_v< Tag, TagT >, bool > = true > T& get() { return *static_cast<T*>(this); }
    };

    template < typename... Types > struct tagged_collection
        : public Types...
    {
        //template < typename... Args > tagged_collection(Args&&... args)
        //    : Types(std::forward< Args >(args))...
        //{}

        tagged_collection(const Types&... args)
            : Types(args)...
        {}

        template < typename Tag > using type = std::remove_reference_t< decltype(std::declval< tagged_collection< Types... > >().get< Tag >()) >;

        using Types::get...;
    };












































































}