#pragma once

#include <sqlstl/storage/storage.h>

#include <optional>

namespace sqlstl
{
    template < typename Key > class set_storage : public storage
    {
    public:
        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator()
                : result_(SQLITE_OK)
                , rowid_(0)
            {}

            iterator(iterator&& other)
                : statement_(std::move(other.statement_))
                , result_(other.result_)
                , rowid_(other.rowid_)
            {}

            iterator(iterator&& other, int result)
                : statement_(std::move(other.statement_))
                , result_(result)
                , rowid_(other.rowid)
            {}

            iterator(sqlstl::statement_cache::statement&& statement, int result)
                : statement_(std::move(statement))
                , result_(result)
                , rowid_(result == SQLITE_ROW ? statement_.extract< uint64_t >(1) : 0)
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

            Key operator*()
            {
                assert(result_ == SQLITE_ROW);
                return statement_.extract< Key >(0);
            }

            Key operator*() const { return const_cast<iterator&>(*this).operator*(); }

            iterator& operator++() const
            {
                assert(result_ == SQLITE_ROW);
                result_ = statement_.step();
                rowid_ = result_ == SQLITE_ROW ? statement_.extract< uint64_t >(1) : 0;
                return const_cast< iterator& >(*this);
            }

            int result() const { return result_; }

        private:
            mutable sqlstl::statement_cache::statement statement_;
            mutable uint64_t rowid_;
            mutable int result_;
        };

        set_storage(sqlstl::db& db)
            : db_(db)
            , create_table_(db.prepare(
                "CREATE TABLE " + get_table_type() + "(" +
                "name TEXT NOT NULL," +
                "key " + type_traits< Key >::sqltype + "," +
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
            return "SET_" + type_traits< Key >::cpptype;
        }

        iterator begin(const std::string& name)
        {
            auto stmt = begin_.acquire();
            auto result = stmt(name);
            return iterator(std::move(stmt), result);
        }

        iterator begin(const std::string& name) const { return const_cast< iterator& >(*this).begin(name); }
        
        iterator end(const std::string& name) const { return iterator(statement_cache::null_statement(), SQLITE_DONE); }

        template < typename K > iterator find(const std::string& name, K&& key) const
        {
            auto stmt = find_.acquire();
            auto result = stmt(name, std::forward< K >(key));
            return iterator(std::move(stmt), result);
        }

        template < typename K > bool insert(const std::string& name, K&& key)
        {
            auto stmt = insert_.acquire();
            auto result = stmt(name, key);
            return result == SQLITE_DONE;
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
        sqlstl::statement_cache begin_, insert_;
        mutable sqlstl::statement_cache size_, find_;
    };
}
