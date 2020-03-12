#pragma once

#include <sqlstl/factory.h>
#include <sqlstl/storage/map.h>
#include <sqlstl/set.h>

#include <type_traits>

namespace sqlstl
{
    template < typename Key, typename Value, typename Factory, bool StandardLayout = std::is_standard_layout_v< Value > > class map_base;

    template < typename Key, typename Value, typename Factory > class map_base< Key, Value, Factory, true > 
    {
    public:
        struct value_proxy
        {
            template < typename K > value_proxy(map_base< Key, Value, Factory >& map, K&& key)
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

        private:
            map_base< Key, Value, Factory, true >& map_;
            Key key_;
        };

        map_base(Factory& factory, const std::string& name)
            : table_(factory.template create_storage< map_storage< Key, Value > >())
            , name_(name)
            , factory_(factory)
        {}

        template < typename K > auto operator[](K&& key)
        {
            table_.insert(name_, key, Value());
            return value_proxy(*this, std::forward< K >(key));
        }

        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(typename map_storage< Key, Value >::iterator&& it)
                : it_(std::move(it))
            {}

            iterator(iterator&& other)
                : it_(std::move(other.it_))
            {}

            auto& operator*() { return *it_; }

            bool operator == (const iterator& other) const { return it_ == other.it_; }
            bool operator != (const iterator& other) const { return it_ != other.it_; }

            iterator& operator++() { ++it_; return *this; }

        private:
            typename map_storage< Key, Value >::iterator it_;
        };

        template < typename K > iterator find(K&& key) { return table_.find(name_, std::forward< K >(key)); }
        iterator begin() { return table_.begin(name_); }
        iterator end() { return table_.end(name_); }

        // private:
        template < typename K, typename V > void update(K&& key, V&& value)
        {
            table_.update(name_, std::forward< K >(key), std::forward< V >(value));
        }

        template < typename K > Value value(K&& key)
        {
            return table_.value(name_, std::forward< K >(key));
        }

        size_t size()
        {
            return table_.size(name_);
        }

        Factory& factory_;
        std::string name_;
        map_storage< Key, Value >& table_;
    };

    template < typename Key, typename Value, typename Factory > class map_base< Key, Value, Factory, false > 
    {
    public:
        map_base(Factory& factory, const std::string& name)
            : factory_(factory)
            , name_(name)
            , set_(factory, name)
        {}

        struct value_proxy
            : public Value
        {
            friend class map_base< Key, Value, Factory >;

            template < typename K > value_proxy(map_base< Key, Value, Factory >& map, K&& key)
                : Value(map.factory_, map.name_ + "." + std::to_string(key))
                , map_(map)
                , key_(std::forward< K >(key))
            {}

            // value_proxy(const value_proxy&) = delete;

            /*
            value_proxy(value_proxy&& other)
                : Value(std::move(other))
                , map_(std::move(other.map_))
                , key_(std::move(other.key_))
            {}
            */

            template < typename V > value_proxy& operator = (V&& value)
            {
                // TODO: Assignment means V is part of this map now, all its fields need to be renamed.
                std::abort();
                return *this;
            }
        private:
            map_base< Key, Value, Factory >& map_;
            Key key_;
        };

        template < typename K > value_proxy operator[](K&& key)
        {
            set_.insert(K(key));
            return value_proxy(*this, std::forward< K >(key));
        }

        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(map_base< Key, Value, Factory >& map, typename set< Key, Factory >::iterator&& it)
                : it_(std::move(it))
                , map_(map)
            {}

            iterator(iterator&& other)
                : it_(std::move(other.it_))
                , pair_(std::move(other.pair_))
            {}

            auto& operator*()
            {
                if(!pair_)
                {
                    pair_.emplace(*it_, Value(map_.factory_, map_.name_ + "." + std::to_string(*it_)));
                }

                return *pair_;
            }

            bool operator == (const iterator& other) const { return it_ == other.it_; }
            bool operator != (const iterator& other) const { return it_ != other.it_; }

            iterator& operator++() { pair_.reset(); ++it_; return *this; }

        private:
            typename set< Key, Factory >::iterator it_;
            map_base< Key, Value, Factory >& map_;
            std::optional< std::pair< const Key, Value > > pair_;
        };

        template < typename K > iterator find(K&& key) { return iterator(*this, set_.find(std::forward< K >(key))); }
        iterator begin() { return iterator(*this, set_.begin()); }
        iterator end() { return iterator(*this, set_.end()); }
        size_t size() { return set_.size(); }

        set< Key, Factory > set_;
        Factory& factory_;
        std::string name_;
    };

    template < typename Key, typename Value, typename Factory > class map
        : public map_base< Key, Value, Factory >
    {
    public:
        map(Factory& factory, const std::string& name)
            : map_base< Key, Value, Factory >(factory, name)
        {}
    };

    template < typename Key, typename Value, typename Factory > struct container_traits< sqlstl::map< Key, Value, Factory > >
    {
        static const bool has_storage_type = true;
    };
}