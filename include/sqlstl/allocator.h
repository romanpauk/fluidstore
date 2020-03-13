#pragma once

#include <sqlstl/sqlite.h>
#include <sqlstl/storage/storage.h>

#include <map>
#include <typeindex>
#include <memory>

namespace sqlstl
{
    template < typename Container > struct container_traits { static const bool has_storage_type = false; };

    class allocator
    {
    public:
        allocator(sqlstl::db& db)
            : db_(db)
        {}

        template < typename Container > Container create_container()
        {
            return Container();
        }

        template < typename Container > Container create_container(const std::string& name)
        {
            if constexpr (container_traits< Container >::has_storage_type)
            {
                return Container(*this, name);
            }
            else
            {
                return Container();
            }
        }

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
}