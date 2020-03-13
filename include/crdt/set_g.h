#pragma once

namespace crdt
{
    template < typename T, typename Traits > class set_g
    {
        template < typename It > class iterator_base
        {
        public:
            iterator_base(typename It&& it)
                : it_(std::move(it))
            {}

            iterator_base(iterator_base< It >&& other)
                : it_(std::move(other.it_))
            {}

            bool operator == (const iterator_base< It >& other) const { return it_ == other.it_; }
            bool operator != (const iterator_base< It >& other) const { return it_ != other.it_; }

            T operator*() const { return *it_; }

            iterator_base< It >& operator++() { ++it_; return *this; }

        private:
            typename It it_;
        };

    public:
        typedef iterator_base< typename Traits::template Set< T, typename Traits::Allocator >::iterator > iterator;
        typedef iterator_base< typename Traits::template Set< T, typename Traits::Allocator >::const_iterator > const_iterator;

        set_g(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : values_(allocator.template create_container< typename Traits::template Set< T, typename Traits::Allocator > >(name))
        {}

        iterator begin() { return values_.begin(); }
        const_iterator begin() const { return values_.begin(); }

        iterator end() { return values_.end(); }
        const_iterator end() const { return values_.end(); }

        template < typename K > std::pair< iterator, bool > insert(K&& value)
        {
            auto pairb = values_.insert(value);
            return { std::move(pairb.first), pairb.second };
        }

        size_t size() const { return values_.size(); }

        template < typename Set > void merge(const Set& other)
        {
            values_.insert(other.begin(), other.end());
        }

        template < typename Set > bool operator == (const Set& other)
        {
            return values_ == other.values_;
        }

    private:
        typename Traits::template Set< T, typename Traits::Allocator > values_;
    };

    template < typename T, typename DeltaTraits, typename StateTraits = DeltaTraits > class delta_set_g
        : public set_g< T, StateTraits >
    {
    public:
        delta_set_g(typename StateTraits::Allocator& allocator, const std::string& name = "")
            : set_g< T, StateTraits >(allocator, name)
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