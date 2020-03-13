#pragma once

namespace crdt
{
    template < typename T, typename Traits > class set_g
    {
    public:
        class iterator
        {
        public:
            iterator(typename Traits::template Set< T, typename Traits::Factory >::iterator&& it)
                : it_(std::move(it))
            {}

            iterator(iterator&& other)
                : it_(std::move(other.it_))
            {}

            bool operator == (const iterator& other) const { return it_ == other.it_; }
            bool operator != (const iterator& other) const { return it_ != other.it_; }

            T operator*() { return *it_; }

            iterator& operator++() { ++it_; return *this; }

        private:
            typename Traits::template Set< T, typename Traits::Factory >::iterator it_;
        };

        set_g(typename Traits::Factory& factory = factory::static_factory(), const std::string& name = "")
            : values_(factory.template create_container< typename Traits::template Set< T, typename Traits::Factory > >(name))
        {}

        iterator begin() { return iterator(values_.begin()); }
        iterator end() { return iterator(values_.end()); }
        
        template < typename K > std::pair< iterator, bool > insert(K&& value)
        {
            auto pairb = values_.insert(value);
            return { std::move(pairb.first), pairb.second };
        }

        size_t size()
        {
            return values_.size();
        }

        template < typename Set > void merge(Set& other)
        {
            values_.insert(other.begin(), other.end());
        }

        template < typename Set > bool operator == (const Set& other)
        {
            return values_ == other.values_;
        }

    private:
        typename Traits::template Set< T, typename Traits::Factory > values_;
    };

    template < typename T, typename DeltaTraits, typename StateTraits = DeltaTraits, typename Factory = factory > class delta_set_g
        : public set_g< T, StateTraits >
    {
    public:
        delta_set_g(typename StateTraits::Factory& factory, const std::string& name = "")
            : set_g< T, StateTraits >(factory, name)
        {}

        set_g< T, DeltaTraits > insert(T value)
        {
            set_g< T, DeltaTraits > delta;
            delta.insert(value);
            this->merge(delta);
            return delta;
        }
    };
}