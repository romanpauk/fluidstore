#pragma once

#include <sqlstl/storage/tuple.h>

namespace sqlstl
{
    template < typename Allocator, typename... Args > class tuple
    {
    public:
        tuple(Allocator&& allocator)
            : allocator_(std::forward< Allocator >(allocator))
            , storage_(&allocator_.template create_storage< tuple_storage< Args... > >())
        {}

        template < typename... V > tuple< Allocator, Args... >& operator = (std::tuple< V... >&& v)
        {
            storage_->replace(allocator_, std::forward< std::tuple< V... > >(v));
            return *this;
        }

        operator std::tuple< Args... >()
        {
            return storage_->value(allocator_);
        }

        auto get_allocator() const { return allocator_; }

    private:
        Allocator allocator_;
        tuple_storage< Args... >* storage_;
    };
}

namespace std
{
    template<size_t I, typename Allocator, typename... T>
    auto get(sqlstl::tuple<Allocator, T...>& value)
    {
        return std::get<I>(static_cast< std::tuple< T... > >(value));
    }
}

