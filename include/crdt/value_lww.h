#pragma once

#include <sqlstl/tuple.h>

namespace crdt
{
    template < typename T, typename Traits > class value_lww
    {
    public:
        value_lww(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : value_(allocator.template create_container< typename Traits::template Tuple< typename Traits::Allocator, uint64_t, T > >(name))
        {}

        operator T() 
        {
            return std::get< 1 >(value_);
        }

        template < typename V > value_lww< T, Traits >& operator = (V&& v)
        {
            value_ = std::make_tuple(0, std::forward< V >(v));
            return *this;
        }

        template < typename Value > void merge(const Value& other)
        {
            if (std::get< 0 >(value_) < std::get< 0 >(other))
            {
                value_ = other;
            }
        }

    private:
        typename Traits::template Tuple< typename Traits::Allocator, uint64_t, T > value_;
    };
}
