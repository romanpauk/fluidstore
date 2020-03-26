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

    template < typename T > class allocator
    {
        template < typename T > friend class allocator;

    public:
        typedef T value_type;

        allocator(factory& factory)
            : factory_(factory)
        {}

        template < typename Allocator > allocator(Allocator&& allocator)
            : factory_(allocator.factory_)
            , name_(allocator.name_)
        {}

        template < typename Allocator > allocator(Allocator&& allocator, const std::string& name)
            : factory_(allocator.factory_)
            , name_(allocator.name_ + "." + name)
        {}

        template < typename Type > Type create(const std::string& name)
        {
            return Type(allocator(factory_, name));
        }

        template < typename Type > Type create()
        {
            return Type(allocator(factory_, std::to_string(rand())));
        }
        
        template < typename Storage > Storage& create_storage() { return factory_.create_storage< Storage >(); }

        const std::string& get_name() const { return name_; }

    private:
        allocator(factory& factory, const std::string& name)
            : factory_(factory)
            , name_(name)
        {}

        factory& factory_;
        std::string name_;
    };
}