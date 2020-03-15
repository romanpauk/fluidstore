#pragma once

#include <crdt/set_or.h>

namespace crdt
{
    template < typename T, typename Traits > class value_mv
    {
    public:
        value_mv(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : values_(allocator, name)
        {}

        operator T()
        {
            if (values_.size() == 1)
            {
                return *values_.begin();
            }
            else
            {
                // throw
                std::abort();
            }
        }

        template < typename V > value_mv< T, Traits >& operator = (V&& v)
        {
            values_.clear();
            values_.insert(std::forward< V >(v));
            return *this;
        }

        template < typename Value > void merge(const Value& other)
        {
            values_.merge(other);
        }

    private:
        set_or< T, Traits > values_;
    };
}
