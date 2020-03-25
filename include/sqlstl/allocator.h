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
    public:
        typedef T value_type;

        allocator(factory& factory)
            : factory_(factory)
        {}

        template < size_t N > allocator(factory& factory, const char(&name)[N])
            : factory_(factory)
            , name_(name)
        {}

        template < typename Allocator > allocator(Allocator&& allocator)
            : factory_(allocator.factory_)
            , name_(allocator.name_)
        {}

        template < typename Allocator, size_t N > allocator(Allocator&& allocator, const char (&name)[N])
            : factory_(allocator.factory_)
            , name_(allocator.name_ + "." + name)
        {}

        template < typename Allocator, typename Value > allocator(Allocator&& allocator, const Value& value, bool replace = false)
            : factory_(allocator.factory_)
            , name_(allocator.name_ + ".")
        {
            if constexpr (type_traits< Value >::embeddable)
            {
                std::stringstream stream;
                stream << value;
                if (!replace)
                {
                    name_ += stream.str();
                }
                else
                {
                    name_ = stream.str();
                }
            }
            else
            {
                name_ += value.get_allocator().get_name();
            }
        }
        
        template < typename Storage > Storage& create_storage() { return factory_.create_storage< Storage >(); }

        const std::string& get_name() const { return name_; }

    // private:
        factory& factory_;
        std::string name_;
    };
}