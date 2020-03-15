#pragma once

#include <sqlstl/sqlite.h>
#include <sqlstl/storage/storage.h>

namespace sqlstl
{
    namespace detail
    {
        template < typename Fn > 
        constexpr void for_each_type(std::tuple<>*, Fn&&) {}
        
        template < typename Fn, typename Head > 
        constexpr void for_each_type(std::tuple< Head >*, Fn&& fn)
        {
            fn((Head*)0);
        }
        
        template < typename Fn, typename Head, typename... Tail > 
        constexpr void for_each_type(std::tuple< Head, Tail...>*, Fn&& fn)
        {
            fn((Head*)0);
            for_each_type((std::tuple< Tail... >*)0, std::forward< Fn >(fn));
        }

        template < typename T, typename Fn > 
        constexpr void for_each_impl(T& tuple, Fn&& fn, std::integral_constant< std::size_t, 0 >)
        {
            fn(std::get< 0 >(tuple));
        }

        template < typename T, typename Fn, std::size_t I > 
        constexpr void for_each_impl(T& tuple, Fn&& fn, std::integral_constant< std::size_t, I >)
        {
            fn(std::get< I >(tuple));
            for_each_impl(tuple, std::forward< Fn >(fn), std::integral_constant< std::size_t, I - 1>{});
        }

        template < typename Fn, typename... Args >
        constexpr void for_each(std::tuple< Args... >& tuple, Fn&& fn)
        {
            for_each_impl(tuple, std::forward< Fn >(fn), std::integral_constant<std::size_t, sizeof...(Args) - 1>{});
        }
    }

    template < typename... Args > class tuple_storage
        : public storage
    {
        typedef std::tuple< Args... > tuple_type;

    public:
        tuple_storage(sqlstl::db& db)
            : create_table_(db.prepare(
                "CREATE TABLE " + get_table_type() + "(" +
                "name TEXT NOT NULL, " +
                get_table_columns() +
                "PRIMARY KEY(name)" +
                ");"
            ))
            , replace_(db, "REPLACE INTO " + get_table_type() + " VALUES(?," + get_table_values() + ");")
            , select_(db, "SELECT * FROM " + get_table_type() + " WHERE name=?;")
        {
            create_table_();
        }

        template < typename... V > void replace(const std::string& name, std::tuple< V... >&& v)
        {
            auto stmt = replace_.acquire();
            auto result = std::apply([&](auto&&... args) { return stmt(name, args...); }, std::move(v));
            assert(result == SQLITE_DONE);
        }

        std::tuple< Args... > value(const std::string& name)
        {
            auto stmt = select_.acquire();
            auto result = stmt(name);
            assert(result == SQLITE_ROW);

            int index = 0;
            std::tuple< Args... > values;
            detail::for_each(values, [&](auto& p)
            {
                ++index;
                p = stmt.extract< std::remove_reference_t< decltype(p) > >(index);
            });
            return values;
        }

    private:
        static std::string get_table_type()
        {
            std::string name = "TUPLE";
            detail::for_each_type((tuple_type*)0, [&](auto* p)
            {
                name += "_" + sqlstl::template type_traits< std::decay_t< decltype(*p) > >::cpptype;
            });
            return name;
        }

        static std::string get_table_columns()
        {
            int index = 0;
            std::string columns;
            detail::for_each_type((tuple_type*)0, [&](auto* p)
            {
                columns += "value" + std::to_string(index++) + " " + sqlstl::type_traits< std::decay_t< decltype(*p) > >::sqltype + ", ";
            });
            return columns;
        }

        static std::string get_table_values()
        {
            std::string values;
            detail::for_each_type((tuple_type*)0, [&](auto* p)
            {
                if (!values.empty()) values += ",";
                values += "?";
            });
            return values;
        }

        sqlstl::statement create_table_;
        sqlstl::statement_cache replace_, select_;
    };
}
