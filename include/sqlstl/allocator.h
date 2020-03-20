#pragma once

#include <sqlstl/sqlite.h>
#include <sqlstl/storage/storage.h>

#include <map>
#include <typeindex>
#include <memory>

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

    template < typename T > class allocator
    {
    public:
        typedef T value_type;

        allocator(factory& factory)
            : factory_(factory)
        {}

        allocator(factory& factory, const std::string& name)
            : factory_(factory)
            , name_(name)
        {}

        template < typename Allocator > allocator(Allocator&& allocator)
            : factory_(allocator.factory_)
            , name_(allocator.name_)
        {}

        template < typename Allocator > allocator(Allocator&& allocator, const std::string& name)
            : factory_(allocator.factory_)
            , name_(allocator.name_ + "." + name)
        {}
        
        template < typename Storage > Storage& create_storage() { return factory_.create_storage< Storage >(); }

        const std::string& get_name() const { return name_; }

    // private:
        factory& factory_;
        std::string name_;
    };
}