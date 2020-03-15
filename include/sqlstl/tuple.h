#pragma once

#include <sqlstl/storage/tuple.h>

namespace sqlstl
{
    template < typename Allocator, typename... Args > class tuple
    {
    public:
        tuple(Allocator& allocator, const std::string& name)
            : storage_(allocator.template create_storage< tuple_storage< Args... > >())
            , name_(name)
        {}

        template < typename... V > tuple< Allocator, Args... >& operator = (std::tuple< V... >&& v)
        {
            storage_.replace(name_, std::forward< std::tuple< V... > >(v));
            return *this;
        }

        std::tuple< Args... > value()
        {
            return storage_.value(name_);
        }

    private:
        tuple_storage< Args... >& storage_;
        std::string name_;
    };

    template < typename Allocator, typename... Args > struct container_traits< tuple< Allocator, Args... > >
    {
        static const bool has_storage_type = true;
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

