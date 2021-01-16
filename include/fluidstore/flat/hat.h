#pragma once

// Hashed Array Trees: http://dns.uls.cl/~ej/daa_08/Algoritmos/books/book10/9609n/9609n.htm
// Unfortunatelly flat::hat is much slower than flat::vector for items that are memcpy-copyable.

#include <fluidstore/flat/memory.h>

namespace crdt::flat
{
    template < typename T > class hat_base
    {
    public:
        using size_type = uint32_t;

        hat_base()
            : top_()
            , power_()
            , size_()
        {}

        ~hat_base()
        {
            assert(!top_);
        }

        T& operator [](size_type n)
        {
            assert(n < size_);
            
            size_type top = top_index(power_, n);
            assert(top < block_size(power_));
            assert(top_[top]);

            size_type leaf = leaf_index(power_, n);
            assert(leaf < block_size(power_));
            
            return top_[top][leaf];
        }

        template < typename Allocator > void push_back(Allocator& allocator, const T& value)
        {
            reserve(allocator, size_ + 1);
            
            size_type top = top_index(power_, size_);
            if (!top_[top])
            {
                auto leaf_alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

                top_[top] = leaf_alloc.allocate(block_size(power_));
                top_[top][0] = value;
            }
            else
            {
                top_[top][leaf_index(power_, size_)] = value;
            }

            size_ += 1;
        }

        size_type size() const { return size_; }
        bool empty() const { return size_ == 0; }

        size_type capacity() const
        {
            return power_ ? block_size(power_) * block_size(power_) : 0;
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            if (top_)
            {
                auto top_alloc = std::allocator_traits< Allocator >::rebind_alloc< T* >(allocator);
                auto leaf_alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

                for (size_type i = 0; i < block_size(power_); ++i)
                {
                    if (top_[i])
                    {
                        leaf_alloc.deallocate(top_[i], block_size(power_));
                    }
                    else
                    {
                        break;
                    }
                }

                top_alloc.deallocate(top_, block_size(power_));
                top_ = nullptr;
            }
        }

    private:
        size_type top_index(size_type power, size_type n) const { return n >> power; }
        size_type leaf_index(size_type power, size_type n) const { return n & (block_size(power) - 1); }
        size_type block_size(size_type power) const { return 1 << power; }

        template < typename Allocator > void reserve(Allocator& allocator, size_type count)
        {
            if (capacity() < count)
            {
                auto top_alloc = std::allocator_traits< Allocator >::rebind_alloc< T* >(allocator);
                
                size_type power = power_ + 1;
                T** top = top_alloc.allocate(block_size(power));                
                memset(top, 0, block_size(power) * sizeof(T*));

                if (top_)
                {
                    auto leaf_alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

                    size_type remaining = size_;

                    for (size_type i = 0; i < power_; i += 2)
                    {
                        if (top_[i])
                        {
                            top[i >> 1] = leaf_alloc.allocate(block_size(power));
                            
                            size_type elements = std::min(remaining, block_size(power_));
                            move_uninitialized(allocator, top[i >> 1], top_[i], elements);
                            remaining -= elements;

                            leaf_alloc.deallocate(top_[i], block_size(power_));
                        }

                        if (top_[i + 1])
                        {
                            size_type elements = std::min(remaining, block_size(power_));
                            move_uninitialized(allocator, top[i >> 1] + block_size(power_), top_[i + 1], elements);
                            remaining -= elements;

                            leaf_alloc.deallocate(top_[i + 1], block_size(power_));
                        }
                    }

                    if (remaining)
                    {

                    }

                    top_alloc.deallocate(top_, block_size(power_));
                }

                top_ = top;
                power_ = power;
            }
        }

        T** top_;
        size_type power_;
        size_type size_;
    };
}