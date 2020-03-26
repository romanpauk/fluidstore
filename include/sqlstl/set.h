#pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/storage/set.h>
#include <sqlstl/iterator.h>

namespace sqlstl
{
    template < typename Key, typename Allocator > class set
    {
        template < typename Container, typename Iterator > friend class iterator;

    public:
        typedef ::sqlstl::iterator< set< Key, Allocator >, typename set_storage< Key >::iterator > iterator;
        typedef iterator const_iterator;
        typedef Key value_type;
        typedef Allocator allocator_type;

        set(Allocator allocator)
            : allocator_(allocator)
            , storage_(allocator.template create_storage< set_storage< Key > >())
        {}

        template < typename K > iterator find(K&& key) const
        {
            auto it = storage_.find(allocator_, key);
            return { this, std::forward< K >(key), std::move(it) };
        }
        
        template < typename K > std::pair< iterator, bool > insert(K&& key)
        {
            auto inserted = storage_.insert(allocator_, key);
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

        iterator begin() { return { this, storage_.begin(allocator_) }; }
        const_iterator begin() const { return { this, storage_.begin(allocator_) }; }

        iterator end() { return { this, storage_.end(allocator_) }; }
        const_iterator end() const { return { this, storage_.end(allocator_) }; }

        size_t size() const { return storage_.size(allocator_); }
        bool empty() const { return size() == 0; }

        auto get_allocator() const { return allocator_; }

    private:
        template < typename K > auto get_storage_iterator(K&& key) const { return storage_.find(allocator_.get_name(), std::forward< K >(key)); }
        auto get_value_type(typename set_storage< Key >::iterator& it) const { return *it; }

        Allocator allocator_;
        set_storage< Key >& storage_;
    };
}