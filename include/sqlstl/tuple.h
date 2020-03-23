#pragma once

#include <sqlstl/storage/tuple.h>

namespace sqlstl
{
    template < typename... Args > class tuple
    {
    public:
        template < typename Allocator > tuple(Allocator&& allocator)
            : allocator_(std::forward< Allocator >(allocator))
            , storage_(allocator_.template create_storage< tuple_storage< Args... > >())
        {}

        template < typename... V > tuple< Args... >& operator = (std::tuple< V... >&& v)
        {
            storage_.replace(allocator_.get_name(), std::forward< std::tuple< V... > >(v));
            return *this;
        }

        std::tuple< Args... > value()
        {
            return storage_.value(allocator_.get_name());
        }

    private:
        allocator< void > allocator_;
        tuple_storage< Args... >& storage_;
    };
}

namespace std
{
    template<size_t I, typename... T>
    auto get(sqlstl::tuple<T...>& t)
    {
        return std::get<I>(t.value());
    }
}

