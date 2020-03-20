#pragma once

#include <sqlstl/storage/tuple.h>

namespace sqlstl
{
    template < typename... Args > class tuple
    {
    public:
        template < typename Allocator > tuple(Allocator&& allocator)
            : storage_(allocator.template create_storage< tuple_storage< Args... > >())
            , allocator_(allocator)
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
        tuple_storage< Args... >& storage_;
        allocator< void > allocator_;
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

