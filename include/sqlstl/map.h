#pragma once

#include <sqlstl/Allocator.h>
#include <sqlstl/storage/map.h>
#include <sqlstl/set.h>
#include <sqlstl/iterator.h>

#include <type_traits>

namespace sqlstl
{
    template < typename Key, typename Value, typename Allocator, bool IsStandardLayout = std::is_standard_layout_v< Value > > class map_base;

    template < typename Key, typename Value, typename Allocator > class map_base< Key, Value, Allocator, true > 
    {
    public:
        typedef ::sqlstl::iterator< map_base< Key, Value, Allocator >, typename map_storage< Key, Value >::iterator > iterator;
        typedef iterator const_iterator;
        typedef std::pair< const Key, const Value > value_type;
        typedef Allocator allocator_type;

        struct value_proxy
        {
            template < typename K > value_proxy(map_base< Key, Value, Allocator >& map, K&& key)
                : map_(map)
                , key_(std::forward< K >(key))
            {}

            template < typename V > value_proxy& operator = (V&& value)
            {
                map_.update(key_, std::forward< V >(value));
                return *this;
            }

            operator Value()
            {
                return map_.value(key_);
            }

            template < typename V > value_proxy& operator += (V&& value)
            {
                this->operator = (map_.value(key_) + value);
                return *this;
            }

        private:
            map_base< Key, Value, Allocator, true >& map_;
            Key key_;
        };

        template < typename K > auto operator[](K&& key)
        {
            storage_.insert(allocator_.get_name(), key, Value());
            return value_proxy(*this, std::forward< K >(key));
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

        struct value_proxy
            : public Value
        {
            value_proxy(Value&& value)
                : Value(std::move(value))
            {}

            template < typename V > value_proxy& operator = (V&& value)
            {
                // TODO: Assignment means V is part of this map now, all its fields need to be renamed.
                std::abort();
                return *this;
            }
        };

        template < typename Alloc > map_base(Alloc&& allocator)
            : allocator_(std::forward< Alloc >(allocator))
            , storage_(allocator.create_storage< set_storage< Key > >())
        {}

        template < typename K > value_proxy operator[](K&& key)
        {
            auto value = get_value(key);
            storage_.insert(allocator_.get_name(), std::forward< K >(key));
            return value_proxy(std::move(value));
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
            return Value(Allocator(allocator_, std::to_string(std::forward< K >(key))));
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