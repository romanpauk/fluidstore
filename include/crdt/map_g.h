#pragma once

namespace crdt
{
    //
    // schema
    //     mapg of name, classtype
    // classtype
    //     mapg of name, field
    // fieldtype
    //     name
    //     type, conflict policy
    //
    // classinstance
    //     schema::class
    //     name
    //     mapg of schema::field, value
    // instances
    //     mapg of name, instance
    //
    //   classtype     
    // map< name, // fieldtype
    //    map< name, tuple< name, type, conflictpolicy > >
    // >
    //
    //   classinstance
    // map< name, // variables
    //    map< name, tuple< fieldtype, value >
    // >
    //


    template < typename Key, typename Value, typename Traits > class map_g
    {
    public:
        struct iterator
        {
            iterator(const iterator&) = delete;
            iterator& operator = (const iterator&) = delete;

            iterator(
                map_g< Key, Value, Traits >& map,
                typename set_g< Key, Traits >::iterator&& it
            )
                : it_(std::move(it))
                , map_(map)
            {}

            iterator(
                map_g< Key, Value, Traits >& map,
                typename set_g< Key, Traits >::iterator&& it,
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

            iterator& operator++() { ++it_; pair_.emplace(std::make_pair(*it_, map_.get_value(*it_)));  return *this; }

        private:
            map_g< Key, Value, Traits >& map_;
            typename set_g< Key, Traits >::iterator it_;
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

        map_g(typename Traits::Factory& factory, const std::string& name)
            : factory_(factory)
            , set_(factory, name)
            , name_(name)
        {}

        iterator begin() { return iterator(*this, set_.begin()); }
        iterator end() { return iterator(*this, set_.end()); }
        size_t size() { return set_.size(); }

        template < typename K > value_proxy operator[](K&& key)
        {
            auto value = get_value(key);
            set_.insert(std::forward< K >(key));
            return value_proxy(std::move(value));
        }

        template < typename K, typename V > std::pair< iterator, bool > emplace(K&& key, V&& value)
        {
            auto pairb = emplace(std::forward< K >(key));

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

        template < typename K > iterator find(K&& key) { return iterator(*this, set_.find(std::forward< K >(key))); }

        template < typename Map > void merge(Map& other)
        {
            for (auto&& value : other)
            {
                merge(value.first, value.second);
            }
        }

        template < typename K, typename V > void merge(K&& k, V&& v)
        {
            auto pairb = insert(std::forward< K >(k));
            pairb.first->second.merge(std::forward< V >(v));
        }

        template < typename K > Value get_value(K&& key)
        {
            return Value(factory_, name_ + ".value." + std::to_string(key));
        }

        typename Traits::Factory& factory_;
        set_g< Key, Traits > set_;
        std::string name_;
    };
}
