#pragma once

namespace crdt
{
    template < typename Key, typename Value, typename Traits, bool IsStandardLayout = std::is_standard_layout_v< Value > > class map_g_base;

    template < typename Key, typename Value, typename Traits > class map_g_base< Key, Value, Traits, false >
    {
        template < typename It > struct iterator_base
        {
            iterator_base(const iterator_base< It >&) = delete;
            iterator_base& operator = (const iterator_base< It >&) = delete;

            iterator_base(
                map_g_base< Key, Value, Traits >& map,
                typename It&& it
            )
                : it_(std::move(it))
                , map_(map)
            {}

            iterator_base(
                map_g_base< Key, Value, Traits >& map,
                typename It&& it,
                Value&& value
            )
                : map_(map)
                , it_(std::move(it))
                , pair_(std::pair< const Key, Value >(*it_, std::move(value)))
            {}

            iterator_base(iterator_base< It >&& other)
                : map_(other.map_)
                , it_(std::move(other.it_))
                , pair_(std::move(other.pair_))
            {}

            auto& operator*() { if (!pair_) pair_.emplace(*it_, map_.get_value(*it_)); return *pair_; }
            auto operator->() { return &this->operator*(); }

            bool operator == (const iterator_base< It >& other) const { return it_ == other.it_; }
            bool operator != (const iterator_base< It >& other) const { return it_ != other.it_; }

            iterator_base< It >& operator++() { ++it_; return *this; }

        private:
            map_g_base< Key, Value, Traits >& map_;
            typename It it_;
            std::optional< std::pair< const Key, Value > > pair_;
        };

    public:
        typedef iterator_base< typename set_g< Key, Traits >::iterator > iterator;
        typedef iterator_base< typename set_g< Key, Traits >::const_iterator > const_iterator;

        struct value_proxy
            : public Value
        {
            value_proxy(Value&& value)
                : Value(std::move(value))
            {}

            template < typename V > value_proxy& operator = (V&& value)
            {
                Value::operator = (std::move(value));
                // TODO: Assignment means V is part of this map now, all its fields need to be renamed.
                // std::abort();
                return *this;
            }
        };

        map_g_base(typename Traits::Allocator& allocator, const std::string& name)
            : allocator_(allocator)
            , set_(allocator, name)
            , name_(name)
        {}

        iterator begin() { return iterator(*this, set_.begin()); }
        const_iterator begin() const { return const_iterator(const_cast< map_g_base< Key, Value, Traits >& >(*this), set_.begin()); }

        iterator end() { return iterator(*this, set_.end()); }
        const_iterator end() const { return const_iterator(const_cast< map_g_base< Key, Value, Traits >& >(*this), set_.end()); }

        size_t size() const { return set_.size(); }

        template < typename K > value_proxy operator[](K&& key)
        {
            auto value = get_value(key);
            set_.insert(std::forward< K >(key));
            return value_proxy(std::move(value));
        }

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

        template < typename K > iterator find(K&& key) { return iterator(*this, set_.find(std::forward< K >(key))); }

        template < typename Map > void merge(const Map& other)
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

        template < typename K > Value get_value(K&& key) const
        {
            return Value(allocator_, name_ + ".value." + std::to_string(key));
        }

        typename Traits::Allocator& allocator_;
        set_g< Key, Traits > set_;
        std::string name_;
    };

    template < typename Key, typename Value, typename Traits > class map_g
        : public map_g_base< Key, Value, Traits >
    {
    public:
        map_g(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : map_g_base< Key, Value, Traits >(allocator, name)
        {}
    };
}
