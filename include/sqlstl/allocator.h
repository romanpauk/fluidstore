#pragma once

#include <sqlstl/sqlite.h>
#include <sqlstl/storage/storage.h>
#include <sqlstl/named_context.h>

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
        : public named_context
    {
        template < typename T > friend class allocator;

    public:
        typedef T value_type;

        allocator(factory& factory)
            : factory_(&factory)
        {}

        template < typename Allocator > allocator(Allocator&& allocator)
            : named_context(allocator.get_name())
            , factory_(allocator.factory_)
        {}

        template < typename Allocator > allocator(Allocator&& allocator, const std::string& name)
            : named_context(allocator.get_name() + "." + name)
            , factory_(allocator.factory_)
        {}

        template < typename Type, typename Value > Type create(Value&& value)
        {
            if constexpr (type_traits< Type >::embeddable)
            {
                return std::forward< Value >(value);
            }
            else
            {
                return Type(allocator(factory_, value));
            }
        }

        template < typename Type > Type create()
        {
            return Type(allocator(factory_, create_name()));
        }
        
        template < typename Storage > Storage& create_storage() { return factory_->create_storage< Storage >(); }

    private:
        allocator(factory* factory, const std::string& name)
            : named_context(name)
            , factory_(factory)
        {}

        factory* factory_;
    };
}