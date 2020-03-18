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
        typedef std::pair< const Key, const Value > value_type;

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
            storage_.insert(name_, key, Value());
            return value_proxy(*this, std::forward< K >(key));
        }

        map_base(Allocator& allocator, const std::string& name)
            : storage_(allocator.template create_storage< map_storage< Key, Value > >())
            , name_(name)
        {}

        template < typename K > iterator find(K&& key) { return { this, storage_.find(name_, std::forward< K >(key)) }; }

        iterator begin() { return { this, storage_.begin(name_) }; }
        iterator begin() const { return { this, storage_.begin(name_) }; }

        iterator end() { return { this, storage_.end(name_) }; }
        iterator end() const { return { this, storage_.end(name_) }; }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto inserted = storage_.insert(name_, key, value);
            return std::make_pair(iterator(this, { std::forward< K >(key), std::forward< V >(value) }), inserted);
        }

        // private:
        template < typename K, typename V > void update(K&& key, V&& value)
        {
            storage_.update(name_, std::forward< K >(key), std::forward< V >(value));
        }

        template < typename K > Value value(K&& key)
        {
            return storage_.value(name_, std::forward< K >(key));
        }

        size_t size()
        {
            return storage_.size(name_);
        }

        auto find_value(const value_type& value) const
        {
            return storage_.find(name_, value.first);
        }

        map_storage< Key, Value >& storage_;
        std::string name_;
    };

    template < typename Key, typename Value, typename Allocator > class map_base< Key, Value, Allocator, false > 
    {
    public:
        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(
                map_base< Key, Value, Allocator >& map,
                typename set< Key, Allocator >::iterator&& it
            )
                : it_(std::move(it))
                , map_(map)
            {}

            iterator(
                map_base< Key, Value, Allocator >& map,
                typename set< Key, Allocator >::iterator&& it,
                Value&& value
            )
                : map_(map)
                , it_(std::move(it))
                , pair_(std::pair< const Key, Value >(*it_, std::move(value)))
            {}

            iterator(iterator&& other)
                : map_(other.map_)
                , it_(std::move(other.it_))
                , pair_(std::move(other.pair_))
            {}

            auto& operator*() { if (!pair_) pair_.emplace(*it_, map_.get_value(*it_)); return *pair_; }
            auto operator->() { return &this->operator*(); }

            bool operator == (const iterator& other) const { return it_ == other.it_; }
            bool operator != (const iterator& other) const { return it_ != other.it_; }

            iterator& operator++() { pair_.reset(); ++it_; return *this; }

        private:
            map_base< Key, Value, Allocator >& map_;
            typename set< Key, Allocator >::iterator it_;
            std::optional< std::pair< const Key, Value > > pair_;
        };

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

        typedef iterator const_iterator;

        map_base(Allocator& allocator, const std::string& name)
            : allocator_(allocator)
            , name_(name)
            , set_(allocator, name)
        {}

        template < typename K > value_proxy operator[](K&& key)
        {
            auto value = get_value(key);
            set_.insert(std::forward< K >(key));
            return value_proxy(std::move(value));
        }

        template < typename K > iterator find(K&& key) { return iterator(*this, set_.find(std::forward< K >(key))); }

        iterator begin() { return iterator(*this, set_.begin()); }
        iterator begin() const { return iterator(const_cast<map_base< Key, Value, Allocator >&>(*this), set_.begin()); }

        iterator end() { return iterator(*this, set_.end()); }
        iterator end() const { return iterator(const_cast< map_base< Key, Value, Allocator >& >(*this), set_.end()); }

        size_t size() const { return set_.size(); }

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
            auto value = get_value(key);
            auto pairb = set_.insert(std::forward< K >(key));
            return std::make_pair(iterator(*this, std::move(pairb.first), std::move(value)), pairb.second);
        }

        template < typename K > Value get_value(K& key)
        {
            return Value(allocator_, name_ + ".value." + std::to_string(key));
        }

        Allocator& allocator_;
        set< Key, Allocator > set_;
        std::string name_;
    };

    template < typename Key, typename Value, typename Allocator > class map
        : public map_base< Key, Value, Allocator >
    {
    public:
        map(Allocator& allocator, const std::string& name)
            : map_base< Key, Value, Allocator >(allocator, name)
        {}
    };

    template < typename Key, typename Value, typename Allocator > struct container_traits< sqlstl::map< Key, Value, Allocator > >
    {
        static const bool has_storage_type = true;
    };
}