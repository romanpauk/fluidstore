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

    template < typename Key, typename Value, typename Allocator > class map
    {
    public:
        typedef map_storage< Key, typename type_traits< Value >::dbtype > map_storage_type;
        typedef ::sqlstl::iterator< map< Key, Value, Allocator >, typename map_storage_type::iterator > iterator;
        typedef iterator const_iterator;
        typedef std::pair< const Key, Value > value_type;
        typedef Allocator allocator_type;

        map(Allocator allocator)
            : allocator_(allocator)
            , storage_(allocator.template create_storage< map_storage_type >())
        {}

        template < typename K > auto operator[](K&& key)
        {
            if constexpr (type_traits< Value >::embeddable)
            {
                storage_.insert(allocator_.get_name(), key, make_sql_value(Value()));
                return map_value_proxy< map< Key, Value, Allocator >, type_traits< Value >::embeddable >(*this, std::forward< K >(key));
            }
            else
            {
                auto value = Value(Allocator(allocator_, key));
                storage_.insert(allocator_.get_name(), key, make_sql_value(value));
                return map_value_proxy< map< Key, Value, Allocator >, type_traits< Value >::embeddable >(std::move(value));
            }            
        }
        
        template < typename K > iterator find(K&& key) 
        {
            if constexpr (type_traits< Value >::embeddable)
            {
                return { this, storage_.find(allocator_.get_name(), std::forward< K >(key)) };
            }
            else
            {
                auto it = storage_.find(allocator_.get_name(), key);
                return iterator(this,
                    std::make_pair(std::forward< K >(key), Value(Allocator(allocator_, key))),
                    std::move(it));
            }          
        }

        // template < typename K > const_iterator find(K&& key) const { return { this, storage_.find(allocator_.get_name(), std::forward< K >(key)) }; }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto inserted = storage_.insert(allocator_.get_name(), key, make_sql_value(value));

            if constexpr (type_traits< Value >::embeddable)
            {
                return std::make_pair(iterator(this, { std::forward< K >(key), std::forward< V >(value) }), inserted);
            } 
            else
            {
                return std::make_pair(iterator(this, std::make_pair(std::forward< K >(key), Value(Allocator(allocator_, key)))), inserted);
            }
        }

        iterator begin() { return { this, storage_.begin(allocator_.get_name()) }; }
        const_iterator begin() const { return { this, storage_.begin(allocator_.get_name()) }; }

        iterator end() { return { this, storage_.end(allocator_.get_name()) }; }
        const_iterator end() const { return { this, storage_.end(allocator_.get_name()) }; }

        // private:
        template < typename K, typename V > void update(K&& key, V&& value)
        {
            storage_.update(allocator_.get_name(), std::forward< K >(key), make_sql_value(std::forward< V >(value)));
        }

        template < typename K > Value value(K&& key)
        {
            return storage_.value(allocator_.get_name(), std::forward< K >(key));
        }

        size_t size() const { return storage_.size(allocator_.get_name()); }
        
        auto get_storage_iterator(const value_type& value) const { return storage_.find(allocator_.get_name(), value.first); }

        auto get_value_type(typename map_storage_type::iterator& it) const 
        {
            if constexpr (type_traits< Value >::embeddable)
            {
                return *it;
            }
            else
            {
                auto row = *it;
                return std::make_pair(row.first, Value(Allocator(allocator_, row.second, true)));
            }
        }

        auto get_allocator() const { return allocator_; }

        Allocator allocator_;
        map_storage_type& storage_;
    };
}