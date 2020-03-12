#pragma once

#include <sqlstl/factory.h>
#include <sqlstl/storage/set.h>

namespace sqlstl
{
    template < typename Key, typename Factory > class set
    {
    public:
        set(Factory& factory, const std::string& name)
            : factory_(factory)
            , table_(factory.template create_storage< set_storage< Key > >())
            , name_(name)
        {}

        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(typename set_storage< Key >::iterator&& it)
                : it_(std::move(it))
            {}

            iterator(iterator&& other)
                : it_(std::move(other.it_))
            {}

            auto& operator*()
            {
                return *it_;
            }

            bool operator == (const iterator& other) const { return it_ == other.it_; }
            bool operator != (const iterator& other) const { return it_ != other.it_; }

            iterator& operator++() { ++it_; return *this; }

        private:
            typename set_storage< Key >::iterator it_;
        };

        template < typename K > iterator find(K&& key) { return table_.find(name_, std::forward< K >(key)); }
        template < typename K > std::pair< iterator, bool > insert(K&& key) { return table_.insert(name_, std::forward< K >(key)); }
        
        template < typename It > void insert(It&& begin, It&& end)
        {
            while (begin != end)
            {
                insert(*begin);
                ++begin;
            }
        }

        iterator begin() { return table_.begin(name_); }
        iterator end() { return table_.end(name_); }
        size_t size() { return table_.size(name_); }
        bool empty() { return size() == 0; }

    private:
        Factory& factory_;
        std::string name_;
        set_storage< Key >& table_;
    };

    template < typename Key, typename Factory > struct container_traits< sqlstl::set< Key, Factory > >
    {
        static const bool has_storage_type = true;
    };
}