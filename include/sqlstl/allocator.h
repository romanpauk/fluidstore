#pragma once

#include <sqlstl/sqlite.h>
#include <sqlstl/storage/storage.h>

#include <map>
#include <typeindex>
#include <memory>
#include <sstream>

namespace sqlstl
{
    class factory
    {
    public:
        factory(sqlstl::db& db)
            : db_(db)
        {}

        template < typename Storage > Storage& create_storage()
        {
            auto& storage = storages_[typeid(Storage)];
            if (!storage)
            {
                storage.reset(new Storage(db_));
            }

            return dynamic_cast<Storage&>(*storage);
        }

    private:
        sqlstl::db& db_;
        std::map< std::type_index, std::unique_ptr< storage > > storages_;
    };

    class named_context
    {
    public:
        named_context() {}

        named_context(const std::string& name)
            : name_(name)
        {}

        const std::string& get_name() const { return name_; }

        static std::string create_name() { return std::to_string(rand()); }

    private:
        std::string name_;
    };

    template < typename T > class allocator
        : public named_context
    {
        template < typename T > friend class allocator;

    public:
        typedef T value_type;

        allocator(factory& factory)
            : factory_(factory)
        {}

        template < typename Allocator > allocator(Allocator&& allocator)
            : named_context(allocator.get_name())
            , factory_(allocator.factory_)
        {}

        template < typename Allocator > allocator(Allocator&& allocator, const std::string& name)
            : named_context(allocator.get_name() + "." + name)
            , factory_(allocator.factory_)
        {}

        template < typename Type > Type create(const std::string& name)
        {
            return Type(allocator(factory_, name));
        }

        template < typename Type > Type create()
        {
            return Type(allocator(factory_, create_name()));
        }
        
        template < typename Storage > Storage& create_storage() { return factory_.create_storage< Storage >(); }

    private:
        allocator(factory& factory, const std::string& name)
            : named_context(name)
            , factory_(factory)
        {}

        factory& factory_;
    };
}