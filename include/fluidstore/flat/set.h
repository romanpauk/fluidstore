#pragma once

#include <fluidstore/flat/vector.h>

namespace crdt::flat
{
    template < typename T, typename SizeType = uint32_t > class set_base
    {
    public:
        using vector_type = vector_base< T, SizeType >;
        using value_type = typename vector_type::value_type;
        using size_type = typename vector_type::size_type;
        using iterator = typename vector_type::iterator;
        using const_iterator = typename vector_type::const_iterator;

        set_base() = default;
        set_base(set_base< T, SizeType >&& other) = default;
        ~set_base() = default;

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

        template < typename Allocator, typename SizeTypeT > void insert(Allocator& allocator, const set_base< T, SizeTypeT >& data)
        {
            if (empty())
            {
                data_.assign(allocator, data.begin(), data.end());
            }
            else
            {
                data_.reserve(allocator, data_.size() + data.size());
                auto it = data_.begin();
                for (auto& value: data)
                {
                    // it = lower_bound(it, value);
                    // it = emplace(allocator, it, value);

                    emplace(allocator, value);
                }
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
            return const_cast<set_base< T >&>(*this).lower_bound(value);
        }

        auto begin() { return data_.begin(); }
        auto begin() const { return data_.begin(); }
        auto end() { return data_.end(); }
        auto end() const { return data_.end(); }

        auto empty() const { return data_.empty(); }
        auto size() const { return data_.size(); }
        auto back() const { return data_.back(); }

        template < typename Allocator > void reserve(Allocator& allocator, size_type count) { data_.reserve(allocator, count); }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            data_.clear(allocator);
        }

        template < typename Ty > void update(iterator it, Ty&& value)
        {
            data_.update(it, std::forward< Ty >(value));
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

    template < typename T, typename Allocator, typename SizeType = uint32_t > class set: private set_base< T, SizeType >
    {
        using set_base_type = set_base< T, SizeType >;

    public:
        using typename set_base_type::iterator;
        using typename set_base_type::const_iterator;
        using typename set_base_type::value_type;
        using size_type = SizeType;

        set(Allocator& allocator)
            : allocator_(allocator)
        {}

        set(set< T, Allocator, SizeType >&& other) = default;

        ~set()
        {
            clear();
        }

        template < typename It > void insert(It begin, It end)
        {
            set_base_type::insert(allocator_, begin, end);
        }

        template < typename Ty > auto insert(Ty&& value)
        {
            return set_base_type::insert(allocator_, end(), std::forward< Ty >(value));
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

        void reserve(size_type count)
        {
            set_base_type::reserve(allocator_, count);
        }

        using set_base_type::begin;
        using set_base_type::end;
        using set_base_type::size;
        using set_base_type::empty;

    private:
        Allocator& allocator_;
    };
}