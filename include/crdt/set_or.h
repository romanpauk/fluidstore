#pragma once
#include <iterator>

namespace crdt
{
    template < typename T, typename Traits > class set_or
    {
        struct set_or_tags
        {
            set_or_tags(typename Traits::Factory& factory = factory::static_factory(), const std::string& name = "")
                : added(factory, name + ".added")
                , removed(factory, name + ".removed")
            {}

            set_g< T, Traits > added;
            set_g< T, Traits > removed;

            template < typename Container > void merge(Container& other)
            {
                added.merge(other.added);
                removed.merge(other.removed);
            }
        };

        typedef uint64_t Tag;

    public:
        set_or(typename Traits::Factory& factory = factory::static_factory(), const std::string& name = "")
            : factory_(factory)
            , values_(factory.template create_container< typename Traits::template Map< T, set_or_tags, typename Traits::Factory > >(name))
            , name_(name)
            , empty_tags_(factory, name + ".emptytags")
        {}

        class iterator
        {
        public:
            iterator(
                typename Traits::template Map< T, set_or_tags, 
                typename Traits::Factory >::iterator&& it
            )
                : it_(std::move(it))
            {}

            bool operator == (const iterator& other) { return it_ == other.it_; }
            bool operator != (const iterator& other) { return it_ != other.it_; }
            iterator& operator++() { ++it_; return *this; }
            T& operator*() { return it_->first; }

        private:
            typename Traits::template Map< T, set_or_tags, typename Traits::Factory >::iterator it_;
        };

        iterator begin() { return values_.begin(); }
        iterator end() { return values_.end(); }

        template < typename K > void insert(K&& value)
        {
            auto it = values_.insert(std::forward< K >(value));
            it.first->second.added.insert(get_tag());
        }

        template < typename It > void insert_erase_node(It& it)
        {
            const auto& data = (*it);
            values_[data.first].merge(data.second);
            erase(data.first);
        }

        template < typename K > void erase(K&& key)
        {
            auto it = values_.find(std::forward< K >(key));
            if (it != values_.end())
            {
                auto& tags = it->second;
                sqlstl::set_difference(
                    tags.added.begin(), tags.added.end(),
                    tags.removed.begin(), tags.removed.end(),
                    sqlstl::inserter(tags.removed)
                );
            }
        }

        template < typename K > bool find(K&& key) 
        {
            auto it = values_.find(std::forward< K >(key));
            if (it != values_.end())
            {
                return is_added(it->second);
            }

            return false;
        }

        template < typename K > auto find_node(K&& key)
        {
            return values_.find(std::forward< K >(key));
        }

        auto end_node()
        {
            return values_.end();
        }

        size_t size()
        {
            size_t count = 0;
            for (auto&& value : values_)
            {
                count += is_added(value.second);
            }
            return count;
        }

        template < typename Set > void merge(/*const*/ Set& other)
        {
            for (auto& element : other.values_)
            {
                values_[element.first].merge(element.second);
            }
        }

        template < typename Set > bool operator == (const Set& other) const
        {
            return values_ == other.values_;
        }

    protected:
        typename Traits::Factory& factory_;
        set_or_tags empty_tags_;

    private:
        Tag get_tag() const { return rand(); }

        template < typename Tags > bool is_added(Tags&& tags)
        {
            return tags.added.size() > tags.removed.size();
        }

    public:
        typename Traits::template Map< T, set_or_tags, typename Traits::Factory > values_;
        std::string name_;
    };

    template < typename T, typename DeltaTraits, typename StateTraits > class delta_set_or
        : public set_or< T, StateTraits >
    {
    public:
        delta_set_or(typename StateTraits::Factory& factory, const std::string& name)
            : set_or< T, StateTraits >(factory, name)
        {}

        set_or< T, DeltaTraits > insert(T value)
        {
            set_or< T, DeltaTraits > delta;
            delta.insert(value);
            this->merge(delta);
            return delta;
        }

        set_or< T, DeltaTraits > erase(T value)
        {
            set_or< T, DeltaTraits > delta;
            auto it = this->find_node(value);
            if (it != this->end_node())
            {
                delta.insert_erase_node(it);
                this->merge(delta);
            }
            return delta;
        }
    };
}