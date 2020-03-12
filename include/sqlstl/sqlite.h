#pragma once

#include "sqlite3.h"

#include <string>
#include <cassert>
#include <stdexcept>
#include <vector>

namespace sqlstl
{
    namespace detail
    {
        template< typename Function, typename... Args, std::size_t... I >
        inline void invoke_with_index(Function&& function, std::integer_sequence<std::size_t, I...>, const Args&... args)
        {
            std::initializer_list<int> { (function(I + 1, args), 0)... };
        }

        template< typename Function, typename... Args>
        inline void invoke_with_index(Function&& function, const Args&... args)
        {
            invoke_with_index(std::forward<Function>(function), std::index_sequence_for<Args...>(), args...);
        }

        struct noncopyable
        {
            noncopyable(const noncopyable&) = delete;
            noncopyable& operator = (const noncopyable&) = delete;
        };
    }

    inline void sqlite3_check(int error)
    {
        if (error != SQLITE_OK)
        {
            throw std::runtime_error(sqlite3_errstr(error));
        }
    }

    class statement
    {
    public:
        statement(const statement&) = delete;
        statement& operator = (const statement&) = delete;

        statement(sqlite3_stmt* stmt = nullptr)
            : stmt_(stmt)
        {}

        statement(statement&& other) noexcept
            : stmt_()
        {
            *this = std::move(other);
        }

        ~statement()
        {
            sqlite3_finalize(stmt_);
        }

        statement& operator = (statement&& other) noexcept
        {
            std::swap(stmt_, other.stmt_);
            return *this;
        }

        template < typename... Args > int operator()(Args&&... args)
        {
            sqlite3_check(sqlite3_reset(stmt_));
            sqlite3_check(sqlite3_clear_bindings(stmt_));

            auto binder = [this](std::size_t index, const auto& value) { bind_parameter(index, value); };
            detail::invoke_with_index(binder, args...);

            return sqlite3_step(stmt_);
        }

        bool operator == (const statement& other) const
        {
            return stmt_ == other.stmt_;
        }

        template < typename V > V extract(int column) const
        {
            V value;
            extract_parameter(column, value);
            return value;
        }

        int step()
        {
            return sqlite3_step(stmt_);
        }

    private:
        void bind_parameter(int index, uint64_t value)
        {
            sqlite3_check(sqlite3_bind_int64(stmt_, index, value));
        }

        void bind_parameter(int index, const std::string& value)
        {
            sqlite3_check(sqlite3_bind_text(stmt_, index, value.c_str(), value.size(), 0));
        }

        void bind_parameter(int index, const char* value)
        {
            sqlite3_check(sqlite3_bind_text(stmt_, index, value, strlen(value), 0));
        }

        void extract_parameter(int index, uint64_t& value) const
        {
            value = sqlite3_column_int64(stmt_, index);
        }

        void extract_parameter(int index, unsigned& value) const
        {
            value = sqlite3_column_int(stmt_, index);
        }

        void extract_parameter(int index, int& value) const
        {
            value = sqlite3_column_int(stmt_, index);
        }

        void extract_parameter(int index, std::string& value) const
        {
            value.assign((const char*)sqlite3_column_text(stmt_, index));
        }

        sqlite3_stmt* stmt_;
    };

    class db
    {
    public:
        db(const std::string& name)
            : db_()
        {
            try
            {
                sqlite3_check(sqlite3_open(name.c_str(), &db_));
            }
            catch (...)
            {
                if (db_)
                {
                    sqlite3_close(db_);
                }

                throw;
            }
        }

        ~db()
        {
            sqlite3_close(db_);
        }

        operator sqlite3* () { return db_; }
        
        statement prepare(const std::string& sql)
        {
            sqlite3_stmt* stmt = nullptr;
            sqlite3_check(sqlite3_prepare_v2(db_, sql.c_str(), sql.size(), &stmt, nullptr));
            return stmt;
        }

    private:
        sqlite3* db_;
    };

    template < typename T > class type_trait;

    template <> struct type_trait< int > 
    {
        inline static const std::string cpptype = "int";
        inline static const std::string sqltype = "INTEGER";
    };

    template <> struct type_trait< uint64_t >
    {
        inline static const std::string cpptype = "uint64_t";
        inline static const std::string sqltype = "INTEGER";
    };

    template <> struct type_trait< std::string >
    {
        inline static const std::string cpptype = "string";
        inline static const std::string sqltype = "TEXT";
    };

    class statement_cache
    {
    public:
        statement_cache(db& db, const std::string& sql)
            : db_(db)
            , sql_(sql)
        {}       

        class statement 
            : public sqlstl::statement
        {
        public:
            statement(const statement&) = delete;

            statement(statement_cache* cache, sqlstl::statement&& stmt)
                : sqlstl::statement(std::move(stmt))
                , cache_(cache)
            {}

            statement(statement&& other)
                : cache_()
            {
                *this = std::move(other);
            }

            ~statement()
            {
                if(cache_)
                {
                    // cache_->statements_.emplace_back(std::move(*this));
                }
            }

            statement& operator = (statement&& other) noexcept
            {
                std::swap(cache_, other.cache_);
                static_cast<sqlstl::statement&>(*this) = std::move(other);
                return *this;
            }

        private:
            statement_cache* cache_;
        };

        statement acquire()
        {
            return statement(this, db_.prepare(sql_));
/*
            if (!statements_.empty())
            {
                statement stmt(this, std::move(statements_.back()));
                statements_.pop_back();
                return stmt;
            }
            else
            {
                return statement(this, db_.prepare(sql_));
            }
*/
        }

    private:
        db& db_;
        std::string sql_;
        std::vector< statement > statements_;
    };

    
}
