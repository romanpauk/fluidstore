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
        using const_iterator = typename vector_type::const_iterator;

        set_base()
        {}

        set_base(set_base< T >&& other)
            : data_(std::move(other.data_))
        {}

        template < typename Allocator, typename Value > std::pair< iterator, bool > emplace(Allocator& allocator, Value&& value)
        {
            if (!data_.empty() && !(*--end() < value))
            {
                auto it = lower_bound(value);
                if (it != end())
                {
                    if (*it == value)
                    {
                        return { it, false };
                    }
                    else
                    {
                        return { data_.emplace(allocator, it, std::forward< Value >(value)), true };
                    }
                }
            }
            
            return { data_.emplace(allocator, data_.end(), std::forward< Value >(value)), true };
        }

        template < typename Allocator > iterator insert(Allocator& allocator, iterator hint, T&& value)
        {
            return insert_impl(allocator, hint, std::move(value));
        }

        template < typename Allocator > iterator insert(Allocator& allocator, iterator hint, const T& value)
        {
            return insert_impl(allocator, hint, value);
        }

        template < typename It, typename Allocator > void insert(Allocator& allocator, It begin, It end)
        {
            data_.reserve(allocator, data_.size() + std::distance(begin, end));
            for (auto it = begin; it != end; ++it)
            {
                emplace(allocator, *it);
            }
        }

        auto find(const T& value)
        {
            return find_impl(value);
        }

        auto find(const T& value) const
        {
            return const_cast< set_base< T >& >(*this).find(value);
        }

        template < typename Allocator > size_type erase(Allocator& allocator, const T& value)
        {
            auto it = find(value);
            if (it != end())
            {
                erase(allocator, it);
                return 1;
            }

            return 0;
        }

        template < typename Allocator > auto erase(Allocator& allocator, iterator it)
        {
            return data_.erase(allocator, it);
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
            return lower_bound_impl(value);
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

    // protected:
        template < typename Allocator, typename Ty > iterator insert_impl(Allocator& allocator, iterator hint, Ty&& value)
        {
            if (!data_.empty() && !(*--end() < value))
            {
                auto it = lower_bound(value);
                if (*it == value)
                {
                    return it;
                }
                else
                {
                    return data_.emplace(allocator, it, std::forward< Ty >(value));
                }
            }

            return data_.emplace(allocator, data_.end(), std::forward< Ty >(value));
        }

        template< typename Ty > auto find_impl(const Ty& value)
        {
            auto it = lower_bound_impl(value);
            if (it != end() && *it == value)
            {
                return it;
            }

            return end();
        }

        template< typename Ty > auto lower_bound_impl(const Ty& value)
        {
            return std::lower_bound(data_.begin(), data_.end(), value);
        }
    private:
        vector_base< T > data_;
    };

    template < typename T, typename Allocator > class set: private set_base< T >
    {
        using set_base_type = set_base< T >;

    public:
        using typename set_base_type::iterator;
        using typename set_base_type::const_iterator;
        using typename set_base_type::value_type;

        set(Allocator& allocator)
            : allocator_(allocator)
        {}

        set(set< T, Allocator >&& other)
            : set_base< T >(std::move(other))
            , allocator_(std::move(other.allocator_))
        {}

        ~set()
        {
            clear();
        }

        template < typename It > void insert(It begin, It end)
        {
            set_base_type::insert(allocator_, begin, end);
        }

        template < typename It, typename Ty > auto insert(It hint, Ty&& value)
        {
            return set_base_type::insert(allocator_, hint, std::forward< Ty >(value));
        }

        void erase(const T& value) 
        { 
            return erase(allocator_, value); 
        }

        void erase(iterator it) 
        { 
            erase(allocator_, it); 
        }

        void clear() 
        { 
            return set_base_type::clear(allocator_); 
        }

        using set_base_type::begin;
        using set_base_type::end;
        using set_base_type::size;
        using set_base_type::empty;

    private:
        Allocator& allocator_;
    };
}