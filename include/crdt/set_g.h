#pragma once

#include <crdt/allocator.h>

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

            const T& operator*() const { return *it_; }

            iterator_base< It >& operator++() { ++it_; return *this; }

        private:
            typename It it_;
        };

        typedef typename Traits::template Set< T > container_type;
        typedef typename container_type::allocator_type allocator_type;

    public:
        typedef iterator_base< typename container_type::iterator > iterator;
        typedef iterator_base< typename container_type::const_iterator > const_iterator;

        template < typename Alloc > set_g(Alloc&& allocator)
            : values_(allocator_type(typename Traits::template Allocator<void>(allocator, "values")))
        {}

        set_g(set_g&& other)
            : values_(std::move(other.values_))
        {}

        iterator begin() { return values_.begin(); }
        const_iterator begin() const { return values_.begin(); }

        iterator end() { return values_.end(); }
        const_iterator end() const { return values_.end(); }

        template < typename K > std::pair< iterator, bool > insert(K&& value)
        {
            auto pairb = values_.insert(std::forward< K >(value));
            return { std::move(pairb.first), pairb.second };
        }

        template < typename It > void insert(It&& from, It&& to)
        {
            while (from != to)
            {
                insert(*from);
                ++from;
            }
        }

        template < typename K > iterator find(K&& key)
        {
            return values_.find(std::forward< K >(key));
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
        container_type values_;
    };
}