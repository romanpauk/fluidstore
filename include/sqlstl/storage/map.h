#pragma once

#include <sqlstl/storage/storage.h>
#include <sqlstl/sqlite.h>

#include <optional>

namespace sqlstl
{
    template < typename Key, typename Value > class map_storage : public storage
    {
    public:
        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(iterator&& other)
                : statement_(std::move(other.statement_))
                , result_(other.result_)
                , rowid_(other.rowid_)
            {}

            iterator(sqlstl::statement_cache::statement&& statement, int result)
                : statement_(std::move(statement))
                , result_(result)
                , rowid_(result == SQLITE_ROW ? statement_.extract< uint64_t >(2) : 0)
            {}

            iterator& operator = (iterator&& other)
            {
                std::swap(statement_, other.statement_);
                std::swap(result_, other.result_);
                std::swap(rowid_, other.rowid_);
                return *this;
            }

            bool operator == (const iterator& other) const { return rowid_ == other.rowid_; }
            bool operator != (const iterator& other) const { return !(*this == other); }

            std::pair< const Key, const Value > operator *()
            {
                assert(result_ == SQLITE_ROW);
                return { statement_.extract< Key >(0), statement_.extract< Value >(1) };
            }

            std::pair< const Key, const Value > operator *() const { return const_cast<iterator&>(*this).operator*(); }

            iterator& operator++()
            {
                assert(result_ == SQLITE_ROW);
                result_ = statement_.step();
                rowid_ = result_ == SQLITE_ROW ? statement_.extract< uint64_t >(2) : 0;
                return *this;
            }

            int result() const { return result_; }

        private:
            sqlstl::statement_cache::statement statement_;
            uint64_t rowid_;
            int result_;
        };

        map_storage(sqlstl::db& db)
            : db_(db)
            , create_table_(db.prepare(
                "CREATE TABLE " + get_table_type() + "(" +
                "name TEXT NOT NULL," +
                "key " + type_traits< Key >::sqltype + " NOT NULL," +
                "value " + type_traits< Value >::sqltype + " NOT NULL," +
                "PRIMARY KEY(name, key)" +
                ");"
            ))
            , begin_(db, "SELECT key, value, rowid FROM " + get_table_type() + " WHERE name=? ORDER BY key ASC;")
            , find_(db, "SELECT key, value, rowid FROM " + get_table_type() + " WHERE name=? AND key>=? ORDER BY key ASC;")
            , update_(db, "UPDATE " + get_table_type() + " SET value=? WHERE name=? AND key=?;")
            , insert_(db, "INSERT INTO " + get_table_type() + " VALUES(?,?,?);")
            , value_(db, "SELECT value FROM " + get_table_type() + " WHERE name=? AND key=?;")
            , size_(db, "SELECT count(*) FROM " + get_table_type() + " WHERE name=?;")
        {
            create_table_();
        }

        static std::string get_table_type()
        {
            return "MAP_" + type_traits< Key >::sqltype + "_" + type_traits< Value >::sqltype;
        }

        iterator begin(const std::string& name)
        {
            auto stmt = begin_.acquire();
            auto result = stmt(name);
            return iterator(std::move(stmt), result);
        }

        iterator end(const std::string& name)
        {
            return iterator(statement_cache::null_statement(), SQLITE_DONE);
        }

        template < typename K > iterator find(const std::string& name, K&& key)
        {
            auto stmt = find_.acquire();
            auto result = stmt(name, std::forward< K >(key));
            return iterator(std::move(stmt), result);
        }

        template < typename K, typename V > void update(const std::string& name, K&& key, V&& value)
        {
            auto stmt = update_.acquire();
            assert(stmt(std::forward< V >(value), name, std::forward< K >(key)) == SQLITE_DONE);
        }

        template < typename K, typename V > bool insert(const std::string& name, K&& key, V&& value)
        {
            auto stmt = insert_.acquire();
            auto result = stmt(name, std::forward< K >(key), std::forward< V >(value));
            return result == SQLITE_DONE;
        }

        template < typename K > Value value(const std::string& name, K&& key)
        {
            auto stmt = value_.acquire();
            if (stmt(name, std::forward< K >(key)) == SQLITE_ROW)
            {
                return stmt.extract< Value >(0);
            }

            std::abort();
        }

        size_t size(const std::string& name)
        {
            auto stmt = size_.acquire();
            stmt(name);
            return stmt.extract< size_t >(0);
        }

    private:
        sqlstl::db& db_;
        sqlstl::statement create_table_;
        sqlstl::statement_cache begin_, find_, update_, insert_, value_, size_;
    };
}
