#pragma once

#include <crdt/set_or.h>

namespace crdt
{
    template < typename T, typename Traits > class value_mv
    {
        template < typename, typename > class value_mv;

    public:
        typedef typename Traits::template Allocator< void > allocator_type;

        value_mv(allocator_type allocator)
            : allocator_(allocator)
            , values_(allocator_type(allocator, "values"))
        {}

        T get()
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

        operator T() { return get(); }

        template < typename V > void /*value_mv< T, Traits >&*/ operator = (V&& v)
        {
            values_.clear();
            values_.insert(std::forward< V >(v));
            // return *this;
        }

        template < typename Value > void merge(const Value& other)
        {
            values_.merge(other.values_);
        }

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
    public:
        set_or< T, Traits > values_;
    };
}
