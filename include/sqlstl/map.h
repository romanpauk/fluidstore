#pragma once

#include <sqlstl/allocator.h>
#include <sqlstl/storage/map.h>
#include <sqlstl/set.h>
#include <sqlstl/iterator.h>

#include <type_traits>

namespace sqlstl
{
    template < typename Container, bool embeddable = type_traits< typename Container::value_type::second_type >::embeddable > struct map_value_proxy;

    template < typename Container > struct map_value_proxy< Container, true >
    {
        template < typename K > map_value_proxy(Container& container, K&& key)
            : container_(container)
            , key_(std::forward< K >(key))
        {}

        template < typename V > auto& operator = (V&& value)
        {
            container_.update(key_, std::forward< V >(value));
            return *this;
        }

        operator typename Container::value_type::second_type()
        {
            return container_.value(key_);
        }

        template < typename V > auto& operator += (V&& value)
        {
            this->operator = (container_.value(key_) + value);
            return *this;
        }

    private:
        Container& container_;
        typename Container::value_type::first_type key_;
    };

    template < typename Container > struct map_value_proxy< Container, false >
        : public Container::value_type::second_type
    {
        typedef typename Container::value_type::second_type Value;

        map_value_proxy(Value&& value)
            : Value(std::move(value))
        {}

        template < typename V > auto& operator = (V&& value)
        {
            Value::operator = (std::forward< V >(value));
            return *this;
        }
    };

    template < typename Key, typename Value, typename Allocator, bool IsEmbeddable = type_traits< Value >::embeddable > class map_base;

    template < typename Key, typename Value, typename Allocator > class map_base< Key, Value, Allocator, true > 
    {
    public:
        typedef ::sqlstl::iterator< map_base< Key, Value, Allocator >, typename map_storage< Key, Value >::iterator > iterator;
        typedef iterator const_iterator;
        typedef std::pair< const Key, const Value > value_type;
        typedef Allocator allocator_type;

        template < typename K > auto operator[](K&& key)
        {
            storage_.insert(allocator_.get_name(), key, make_sql_value(Value()));
            return map_value_proxy< map_base< Key, Value, Allocator >, true >(*this, std::forward< K >(key));
        }

        template < typename Alloc > map_base(Alloc&& allocator)
            : allocator_(std::forward< Alloc >(allocator))
            , storage_(allocator_.template create_storage< map_storage< Key, Value > >())
        {}

        template < typename K > iterator find(K&& key) { return { this, storage_.find(allocator_.get_name(), std::forward< K >(key)) }; }
        template < typename K > const_iterator find(K&& key) const { return { this, storage_.find(allocator_.get_name(), std::forward< K >(key)) }; }

        iterator begin() { return { this, storage_.begin(allocator_.get_name()) }; }
        const_iterator begin() const { return { this, storage_.begin(allocator_.get_name()) }; }

        iterator end() { return { this, storage_.end(allocator_.get_name()) }; }
        const_iterator end() const { return { this, storage_.end(allocator_.get_name()) }; }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto inserted = storage_.insert(allocator_.get_name(), key, value);
            return std::make_pair(iterator(this, { std::forward< K >(key), std::forward< V >(value) }), inserted);
        }

        // private:
        template < typename K, typename V > void update(K&& key, V&& value)
        {
            storage_.update(allocator_.get_name(), std::forward< K >(key), std::forward< V >(value));
        }

        template < typename K > Value value(K&& key)
        {
            return storage_.value(allocator_.get_name(), std::forward< K >(key));
        }

        size_t size() const { return storage_.size(allocator_.get_name()); }
        
        auto get_storage_iterator(const value_type& value) const { return storage_.find(allocator_.get_name(), value.first); }
        auto get_value_type(typename map_storage< Key, Value >::iterator& it) const { return *it; }

        Allocator allocator_;
        map_storage< Key, Value >& storage_;
    };

    template < typename Key, typename Value, typename Allocator > class map_base< Key, Value, Allocator, false > 
    {
    public:
        typedef ::sqlstl::iterator< map_base< Key, Value, Allocator >, typename set_storage< Key >::iterator > iterator;
        typedef iterator const_iterator;
        typedef std::pair< const Key, Value > value_type;
        typedef Allocator allocator_type;

        template < typename Alloc > map_base(Alloc&& allocator)
            : allocator_(std::forward< Alloc >(allocator))
            , storage_(allocator.create_storage< set_storage< Key > >())
        {}

        template < typename K > auto operator[](K&& key)
        {
            auto value = get_value(key);
            storage_.insert(allocator_.get_name(), make_sql_value(key));
            return map_value_proxy< map_base< Key, Value, Allocator >, false >(std::move(value));
        }

        template < typename K > iterator find(K&& key) 
        {
            auto value = get_value(key);
            auto it = storage_.find(allocator_.get_name(), key);
            return iterator(this, 
                std::make_pair(std::forward< K >(key), std::move(value)), 
                std::move(it));
        }

        iterator begin() { return { this, storage_.begin(allocator_.get_name()) }; }
        iterator begin() const { return { this, storage_.begin(allocator_.get_name()) }; }

        iterator end() { return { this, storage_.end(allocator_.get_name()) }; }
        iterator end() const { return { this, storage_.end(allocator_.get_name()) }; }

        size_t size() const { return storage_.size(allocator_.get_name()); }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto pairb = insert(std::forward< K >(key));

            if (pairb.second)
            {
                // TODO: copy values
                // v = std::move(value);
            }

            return pairb;
        }

        template < typename K > std::pair< iterator, bool > insert(K&& key)
        {
            auto inserted = storage_.insert(allocator_.get_name(), key);
            auto value = get_value(key);
            
            return std::make_pair(iterator(this, std::make_pair(std::forward< K >(key), std::move(value))), inserted);
        }

        template < typename K > Value get_value(K&& key) const
        {
            return Value(Allocator(allocator_, key));
        }

        auto get_storage_iterator(const value_type& value) const { return storage_.find(allocator_.get_name(), value.first); }
        auto get_value_type(typename set_storage< Key >::iterator& it) const { return std::make_pair(*it, get_value(*it)); }

        Allocator allocator_;
        set_storage< Key >& storage_;
    };

    template < typename Key, typename Value, typename Allocator > class map
        : public map_base< Key, Value, Allocator >
    {
    public:
        template < typename Alloc > map(Alloc&& allocator)
            : map_base< Key, Value, Allocator >(std::forward< Alloc >(allocator))
        {}
    };
}