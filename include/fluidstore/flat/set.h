#pragma once

#include <fluidstore/flat/vector.h>

namespace crdt::flat
{
    template < typename T > class set_base
    {
    public:
        using vector_type = vector_base< T >;
        using value_type = typename vector_type::value_type;
        using size_type = typename vector_type::size_type;
        using iterator = typename vector_type::iterator;

        set_base()
        {}

        set_base(set_base< T >&& other)
            : data_(std::move(other.data_))
        {}

        template < typename Allocator, typename Ty > std::pair< iterator, bool > emplace(Allocator& allocator, Ty&& value)
        {
            if (!data_.empty() && value < *--data_.end())
            {
                auto it = lower_bound(value);
                if (*it == value)
                {
                    return { it, false };
                }
                else
                {
                    return { data_.emplace(allocator, it, std::forward< Ty >(value)), true };
                }
            }
            
            return { data_.emplace(allocator, data_.end(), std::forward< Ty >(value)), true };
        }

        template < typename Allocator, typename Ty > iterator insert(Allocator& allocator, iterator hint, Ty&& value)
        {
            if (!data_.empty() && value < *--data_.end()) // TOOD: does this check make sense?
            {
                auto it = lower_bound(value);
                if (*it == value)
                {
                    return { it, false };
                }
                else
                {
                    return { data_.emplace(allocator, it, value), true };
                }
            }

            return data_.emplace(allocator, data_.end(), std::forward< Ty >(value));
        }

        template < typename It, typename Allocator > void insert(Allocator& allocator, It begin, It end, size_type size)
        {
            data_.reserve(allocator, data_.size() + size);
            for (auto it = begin; it != end; ++it)
            {
                emplace(allocator, *it);
            }
        }

        auto find(const T& value)
        {
            auto it = lower_bound(value);
            if(it != end() && *it == value)
            {
                return it;
            }

            return end();
        }

        auto find(const T& value) const
        {
            return const_cast< set_base< T >& >(*this).find(value);
        }

        template < typename Allocator > void erase(Allocator& allocator, const T& value)
        {
            auto it = find(value);
            if (it != end())
            {
                erase(allocator, it);
            }
        }

        template < typename Allocator > void erase(Allocator& allocator, iterator it)
        {
            data_.erase(allocator, it);
        }

        iterator upper_bound(const T& value)
        {
            return std::upper_bound(data_.begin(), data_.end(), value);

            for (auto it = data_.begin(); it != data_.end(); ++it)
            {
                if (*it > value)
                    return it;
            }

            return data_.end();
        }

        iterator upper_bound(const T& value) const
        {
            return const_cast<set_base< T >&>(*this).upper_bound(value);
        }

        iterator lower_bound(const T& value)
        {
            return std::lower_bound(data_.begin(), data_.end(), value);
        }

        iterator lower_bound(const T& value) const
        {
            return const_cast<set_base< T >&>(*this).upper_bound(value);
        }

        auto begin() { return data_.begin(); }
        auto begin() const { return data_.begin(); }
        auto end() { return data_.end(); }
        auto end() const { return data_.end(); }

        auto empty() const { return data_.empty(); }
        auto size() const { return data_.size(); }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            data_.clear(allocator);
        }

    private:
        vector_base< T > data_;
    };

    template < typename T, typename Allocator > class set: private set_base< T >
    {
    public:
        using typename set_base< T >::iterator;
        using typename set_base< T >::value_type;

        set(Allocator& allocator)
            : allocator_(allocator)
        {}

        ~set()
        {
            clear(allocator_);
        }

        template < typename It > void insert(It begin, It end, size_type size)
        {
            set_base< T >::insert(allocator_, begin, end, size);
        }

        template < typename It, typename Ty > auto insert(It hint, Ty&& value)
        {
            return set_base< T >::insert(allocator_, hint, std::forward< Ty >(value));
        }

        void erase(Allocator& allocator, const T& value) 
        { 
            return erase(allocator_, value); 
        }

        void erase(iterator it) 
        { 
            erase(allocator_, it); 
        }

        void clear() 
        { 
            return clear(allocator_); 
        }

        using set_base< T >::begin;
        using set_base< T >::end;
        using set_base< T >::size;
        using set_base< T >::empty;

    private:
        Allocator& allocator_;
    };
}