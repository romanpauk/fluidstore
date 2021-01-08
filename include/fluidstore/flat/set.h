#pragma once

#include <fluidstore/flat/vector.h>

namespace crdt::flat
{
    template < typename T > class set
    {
    public:
        using vector_type = vector< T >;
        using size_type = typename vector_type::size_type;
        using iterator = typename vector_type::iterator;

        template < typename Allocator, typename Ty > std::pair< iterator, bool > emplace(Allocator& allocator, Ty&& value)
        {
            if (!data_.empty())
            {
                if (value < *--data_.end())
                {
                    /*
                    auto it = upper_bound(value);
                    if (*it == value)
                    {
                        return { it, false };
                    }
                    else
                    {
                        return { data_.insert(allocator, it, value), true };
                    }
                    */

                    for (auto it = data_.begin(); it != data_.end(); ++it)
                    {
                        if (*it == value)
                        {
                            return { it, false };
                        }
                        else if (*it > value)
                        {
                            return { data_.emplace(allocator, it, std::forward< Ty >(value)), true };
                        }
                    }
                }
            }

            return { data_.emplace(allocator, data_.end(), std::forward< Ty >(value)), true };
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
            return data_.find(value);
        }

        auto find(const T& value) const
        {
            return data_.find(value);
        }

        template < typename Allocator > void erase(Allocator& allocator, const T& value)
        {
            auto it = find(value);
            if (it)
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
            // return std::upper_bound(data_.begin(), data_.end(), value);

            for (auto it = data_.begin(); it != data_.end(); ++it)
            {
                if (*it > value)
                    return it;
            }

            return data_.end();
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
        vector< T > data_;
    };
}