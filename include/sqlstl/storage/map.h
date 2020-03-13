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
                , pair_(std::move(other.pair_))
            {}

            iterator(sqlstl::statement_cache::statement&& statement, int result)
                : statement_(std::move(statement))
                , result_(result)
            {}

            bool operator == (const iterator& other) const
            {
                // TODO: this is not right
                // return std::tie(statement_, result_) == std::tie(other.statement_, other.result_);
                return result_ == other.result_;
            }

            bool operator != (const iterator& other) const
            {
                return !(*this == other);
            }

            std::pair< const Key, Value >& operator *()
            {
                if (!pair_)
                {
                    pair_.emplace(statement_.extract< Key >(0), statement_.extract< Value >(1));
                }

                return *pair_;
            }

            auto operator ->() { return &this->operator*(); }

            iterator& operator++()
            {
                pair_.reset();
                result_ = statement_.step();
                return *this;
            }

        private:
            sqlstl::statement_cache::statement statement_;
            std::optional< std::pair< const Key, Value > > pair_;
            int result_;
        };

        map_storage(sqlstl::db& db)
            : db_(db)
            , create_table_(db.prepare(
                "CREATE TABLE " + get_table_type() + "(" +
                "name TEXT NOT NULL," +
                "key " + type_trait< Key >::sqltype + " NOT NULL," +
                "value " + type_trait< Value >::sqltype + " NOT NULL," +
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
            return "MAP_" + type_trait< Key >::sqltype + "_" + type_trait< Value >::sqltype;
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

        template < typename K, typename V > std::pair< iterator, bool > insert(const std::string& name, K&& key, V&& value)
        {
            auto stmt = insert_.acquire();
            auto result = stmt(name, std::forward< K >(key), std::forward< V >(value));
            auto it = find(name, key);
            assert(it != end(name));
            return { std::move(it), result == SQLITE_DONE };
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
