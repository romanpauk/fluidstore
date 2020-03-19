#pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/storage/set.h>
#include <sqlstl/iterator.h>

namespace sqlstl
{
    template < typename Key, typename Allocator > class set
    {
    public:
        typedef ::sqlstl::iterator< set< Key, Allocator >, typename set_storage< Key >::iterator > iterator;
        typedef iterator const_iterator;
        typedef Key value_type;

        set(Allocator& allocator, const std::string& name)
            : storage_(allocator.template create_storage< set_storage< Key > >())
            , name_(name)
        {}

        template < typename K > iterator find(K&& key) const
        {
            auto it = storage_.find(name_, key);
            return { this, std::forward< K >(key), std::move(it) };
        }
        
        template < typename K > std::pair< iterator, bool > insert(K&& key)
        {
            auto inserted = storage_.insert(name_, key);
            return { iterator(this, std::forward< K >(key)), inserted };
        }
        
        template < typename It > void insert(It&& begin, It&& end)
        {
            while (begin != end)
            {
                insert(*begin);
                ++begin;
            }
        }

        iterator begin() { return { this, storage_.begin(name_) }; }
        const_iterator begin() const { return { this, storage_.begin(name_) }; }

        iterator end() { return { this, storage_.end(name_) }; }
        const_iterator end() const { return { this, storage_.end(name_) }; }

        size_t size() const { return storage_.size(name_); }
        bool empty() const { return size() == 0; }

        template < typename K > auto get_storage_iterator(K&& key) const { return storage_.find(name_, std::forward< K >(key)); }
        auto get_value_type(typename set_storage< Key >::iterator& it) const { return *it; }

    private:
        set_storage< Key >& storage_;
        std::string name_;        
    };

    template < typename Key, typename Allocator > struct container_traits< sqlstl::set< Key, Allocator > >
    {
        static const bool has_storage_type = true;
    };
}