#pragma once

#include <sqlstl/storage/storage.h>

#include <optional>

namespace sqlstl
{
    template < typename Key > class set_storage : public storage
    {
    public:
        set_storage(sqlstl::db& db)
            : db_(db)
            , create_table_(db.prepare(
                "CREATE TABLE " + get_table_type() + "(" +
                "name TEXT NOT NULL," +
                "key " + type_trait< Key >::sqltype + "," +
                "PRIMARY KEY(name, key)" +
                ");"
            ))
            , begin_(db, "SELECT key, rowid FROM " + get_table_type() + " WHERE name=? ORDER BY key ASC;")
            , find_(db, "SELECT key, rowid FROM " + get_table_type() + " WHERE name=? AND key>=? ORDER BY key ASC;")
            , insert_(db, "INSERT INTO " + get_table_type() + " VALUES(?,?);")
            , size_(db, "SELECT count(*) FROM " + get_table_type() + " WHERE name=?;")
        {
            create_table_();
        }

        static std::string get_table_type()
        {
            return "SET_" + type_trait< Key >::cpptype;
        }

        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(iterator&& other)
                : statement_(std::move(other.statement_))
                , result_(other.result_)
            {}

            iterator(iterator&& other, int result)
                : statement_(std::move(other.statement_))
                , result_(result)
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

            const Key& operator*()
            {
                if (!key_)
                {
                    key_ = std::move(statement_.extract< Key >(0));
                }

                return *key_; 
            }

            iterator& operator++()
            {
                key_.reset();
                result_ = statement_.step();
                return *this;
            }

        private:
            std::optional< Key > key_;
            sqlstl::statement_cache::statement statement_;
            int result_;
        };

        iterator begin(const std::string& name)
        {
            auto stmt = begin_.acquire();
            auto result = stmt(name);
            return iterator(std::move(stmt), result);
        }

        iterator end(const std::string& name) const
        {
            return iterator(statement_cache::null_statement(), SQLITE_DONE);
        }

        template < typename K > iterator find(const std::string& name, K&& key)
        {
            auto stmt = find_.acquire();
            auto result = stmt(name, std::forward< K >(key));
            return iterator(std::move(stmt), result);
        }

        template < typename K > std::pair< iterator, bool > insert(const std::string& name, K&& key)
        {
            auto stmt = insert_.acquire();
            auto result = stmt(name, key);
            auto it = find(name, key);
            assert(it != end(name));
            return { std::move(it), result == SQLITE_DONE };
        }

        size_t size(const std::string& name) const
        {
            auto stmt = size_.acquire();
            stmt(name);
            return stmt.extract< size_t >(0);
        }

    private:
        sqlstl::db& db_;
        sqlstl::statement create_table_;
        sqlstl::statement_cache begin_, find_, insert_;
        mutable sqlstl::statement_cache size_;
    };
}
