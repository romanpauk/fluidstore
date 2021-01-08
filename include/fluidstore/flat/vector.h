#pragma once

#include <fluidstore/crdts/noncopyable.h>

namespace crdt::flat
{
    template < typename T > class vector
        : public noncopyable
    {
    public:
        using size_type = uint32_t;

        struct iterator
        {
            // using difference_type = size_type;

            friend class vector< T >;

            iterator(vector< T >* p, size_type index)
                : p_(p)
                , index_(index)
            {}

            T& operator * ()
            {
                assert(index_ < p_->size());
                return (*p_)[index_];
            }

            T* operator -> ()
            {
                return &operator*();
            }

            bool operator == (const iterator& other)
            {
                assert(p_ == other.p_);
                return index_ == other.index_;
            }

            bool operator != (const iterator& other)
            {
                assert(p_ == other.p_);
                return index_ != other.index_;
            }

            iterator& operator++()
            {
                assert(index_ < p_->size());
                ++index_;
                return *this;
            }

            iterator& operator--()
            {
                assert(index_ > 0);
                --index_;
                return *this;
            }

            iterator operator++(int)
            {
                assert(index_ < p_->size());
                return iterator(p_, index_++);
            }

            iterator operator--(int)
            {
                assert(index_ > 0);
                return iterator(p_, index_--);
            }

        private:
            vector< T >* p_;
            size_type index_;
        };

        vector()
            : array_()
            , size_()
            , capacity_()
        {}

        ~vector()
        {
            assert(!array_);
        }

        template < typename Allocator > void reserve(Allocator& allocator, size_type size)
        {
            grow(allocator, size);
        }

        template < typename Allocator > void push_back(Allocator& allocator, const T& value)
        {
            emplace(allocator, end(), value);
        }

        template < typename Allocator, typename... Args > iterator emplace(Allocator& allocator, iterator position, Args&&... args)
        {
            size_type index = position.index_;
            grow(allocator, size_ + 1);
            memmove(array_ + index + 1, array_ + index, (size_ - index) * sizeof(T));
            array_[index] = T(std::forward< Args >(args)...);
            size_ += 1;

            return iterator(this, index);
        }

        iterator find(const T& value)
        {
            for (size_type i = 0; i < size_; ++i)
            {
                if (array_[i] == value)
                {
                    return iterator(this, i);
                }
            }

            return end();
        }

        iterator find(const T& value) const
        {
            return const_cast<vector< T >&>(*this).find(value);
        }

        T& operator [](size_type n)
        {
            assert(n < size_);
            assert(!empty());
            return array_[n];
        }

        template < typename Allocator > void erase(Allocator&, iterator it)
        {
            memmove(array_ + it.index_, array_ + it.index_ + 1, (size_ - it.index_) * sizeof(T));
            size_ -= 1;
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            allocator.deallocate(array_, capacity_);
            array_ = nullptr;
            capacity_ = size_ = 0;
        }

        size_type size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() { return iterator(this, 0); }
        iterator begin() const { return iterator(const_cast<vector< T >*>(this), 0); }
        iterator end() { return iterator(this, size_); }
        iterator end() const { return iterator(const_cast<vector< T >*>(this), size_); }

    private:
        template < typename Allocator > void grow(Allocator& allocator, size_type size)
        {
            if (capacity_ < size)
            {
                size_type capacity = size_type((capacity_ + size) * 1.5);
                T* array = allocator.allocate(capacity);
                if (array_)
                {
                    memcpy(array, array_, size_ * sizeof(T));
                    allocator.deallocate(array_, capacity_); // TODO: deallocate at the end
                }

                capacity_ = capacity;
                array_ = array;
            }
        }

        T* array_;
        size_type size_;
        size_type capacity_;
    };
}
