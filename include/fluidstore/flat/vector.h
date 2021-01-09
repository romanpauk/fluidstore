#pragma once

#include <fluidstore/crdts/noncopyable.h>

#include <iterator>

namespace crdt::flat
{
    template < typename T > class vector_base
        : public noncopyable
    {
    public:
        using size_type = uint32_t;
        using value_type = T;

        struct iterator : public std::iterator< std::random_access_iterator_tag, T >
        {
            friend class vector_base< T >;

            using typename std::iterator< std::random_access_iterator_tag, T >::difference_type;

            iterator(vector_base< T >* p, size_type index)
                : p_(p)
                , index_(index)
            {}

            T& operator * ()
            {
                assert(index_ < p_->size());
                return (*p_)[index_];
            }

            const T& operator * () const
            {
                return const_cast<iterator&>(*this).operator *();
            }

            T* operator -> ()
            {
                return &operator*();
            }

            const T* operator -> () const
            {
                return const_cast<iterator&>(*this).operator ->();
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

            difference_type operator - (const iterator& rhs) const
            {
                return index_ - rhs.index_;
            }

            iterator& operator += (difference_type size)
            {
                // TODO: casts
                assert(index_ + (size_type)size <= p_.size());
                index_ += (size_type)size;
                return *this;
            }

        private:
            vector_base< T >* p_;
            size_type index_;
        };

        vector_base()
            : array_()
            , size_()
            , capacity_()
        {}

        vector_base(vector_base< T >&& other)
            : array_(other.array_)
            , size_(other.size_)
            , capacity_(other.capacity_)
        {
            other.array_ = nullptr;
            other.size_ = other.capacity_ = 0;
        }

        ~vector_base()
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
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

            size_type index = position.index_;
            grow(allocator, size_ + 1);

            // TODO: will have to support moves better
            memmove(array_ + index + 1, array_ + index, (size_ - index) * sizeof(T));
            std::allocator_traits< decltype(alloc) >::construct(alloc, &array_[index], std::forward< Args >(args)...);
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
            return const_cast<vector_base< T >&>(*this).find(value);
        }

        T& operator [](size_type n)
        {
            assert(n < size_);
            assert(!empty());
            return array_[n];
        }

        template < typename Allocator > void erase(Allocator& allocator, iterator it)
        {
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);
            std::allocator_traits< decltype(alloc) >::destroy(alloc, &array_[it.index_]);

            // TODO: moves
            memmove(array_ + it.index_, array_ + it.index_ + 1, (size_ - it.index_) * sizeof(T));
            size_ -= 1;
            if (size_ == 0)
            {
                clear(allocator);
            }
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);
            for (size_type i = 0; i < size_; ++i)
            {
                std::allocator_traits< decltype(alloc) >::destroy(alloc, &array_[i]);
            }

            alloc.deallocate(array_, capacity_);
            array_ = nullptr;
            capacity_ = size_ = 0;
        }

        size_type size() const { return size_; }
        bool empty() const { return size_ == 0; }

        iterator begin() { return iterator(this, 0); }
        iterator begin() const { return iterator(const_cast<vector_base< T >*>(this), 0); }
        iterator end() { return iterator(this, size_); }
        iterator end() const { return iterator(const_cast<vector_base< T >*>(this), size_); }

    private:
        template < typename Allocator > void grow(Allocator& allocator, size_type size)
        {
            if (capacity_ < size)
            {
                auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

                size_type capacity = size_type((capacity_ + size) * 2);
                T* array = alloc.allocate(capacity);
                if (array_)
                {
                    memcpy(array, array_, size_ * sizeof(T));
                    alloc.deallocate(array_, capacity_); // TODO: deallocate at the end
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
