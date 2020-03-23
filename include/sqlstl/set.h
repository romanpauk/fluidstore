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
        typedef Allocator allocator_type;

        set(Allocator&& allocator)
            : allocator_(std::forward< Allocator >(allocator))
            , storage_(allocator_.template create_storage< set_storage< Key > >())
        {}

        template < typename K > iterator find(K&& key) const
        {
            auto it = storage_.find(allocator_.get_name(), key);
            return { this, std::forward< K >(key), std::move(it) };
        }
        
        template < typename K > std::pair< iterator, bool > insert(K&& key)
        {
            auto inserted = storage_.insert(allocator_.get_name(), key);
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

        iterator begin() { return { this, storage_.begin(allocator_.get_name()) }; }
        const_iterator begin() const { return { this, storage_.begin(allocator_.get_name()) }; }

        iterator end() { return { this, storage_.end(allocator_.get_name()) }; }
        const_iterator end() const { return { this, storage_.end(allocator_.get_name()) }; }

        size_t size() const { return storage_.size(allocator_.get_name()); }
        bool empty() const { return size() == 0; }

        template < typename K > auto get_storage_iterator(K&& key) const { return storage_.find(allocator_.get_name(), std::forward< K >(key)); }
        auto get_value_type(typename set_storage< Key >::iterator& it) const { return *it; }

    private:
        Allocator allocator_;
        set_storage< Key >& storage_;
    };
}