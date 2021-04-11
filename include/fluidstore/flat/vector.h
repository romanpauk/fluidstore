#pragma once

#include <fluidstore/flat/memory.h>

#include <iterator>

namespace crdt::flat
{
    template < typename T > class vector_base
    {
    public:
        using size_type = uint16_t;
        using value_type = T;

    private:
        struct vector_data
        {
            size_type size;
            size_type capacity;
            alignas(T) unsigned char buffer[1];

            T* get(size_type n = 0) { return (T*)buffer + n; }
        };

    public:
        struct iterator : public std::iterator< std::random_access_iterator_tag, T, size_type >
        {
            friend class vector_base< T >;

            using typename std::iterator< std::random_access_iterator_tag, T, size_type >::difference_type;

            iterator()
                : p_()
                , index_()
            {}

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
            bool operator == (const iterator& other) const
            {
                assert(p_ == other.p_);
                return index_ == other.index_;
            }

            bool operator != (const iterator& other) const
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
                assert(index_ + (size_type)size <= p_->size());
                index_ += (size_type)size;
                return *this;
            }

        private:
            vector_base< T >* p_;
            size_type index_;
        };

        // TODO:
        using const_iterator = iterator;

        vector_base()
            : data_()
        {}

        vector_base(vector_base< T >&& other)
            : data_(other.data_)
        {
            other.data_ = nullptr;
        }

        vector_base(const vector_base< T >&) = delete;
        vector_base< T >& operator = (const vector_base< T >&) = delete;        
        ~vector_base() = default;
        
        template < typename Allocator > void reserve(Allocator& allocator, size_type nsize)
        {
            grow(allocator, nsize);
        }

        template < typename Allocator > void push_back(Allocator& allocator, const T& value)
        {
            emplace(allocator, end(), value);
        }

        template < typename Allocator, typename... Args > iterator emplace(Allocator& allocator, iterator position, Args&&... args)
        {
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

            size_type index = position.index_;
            grow(allocator, size() + 1); // TODO: this is a bit stupid, we copy first everything and than move it again
            move(alloc, data_->get(index + 1), data_->get(index), size() - index);
            std::allocator_traits< decltype(alloc) >::construct(alloc, data_->get(index), std::forward< Args >(args)...);
            ++data_->size;

            return iterator(this, index);
        }

        iterator find(const T& value)
        {
            for (size_type i = 0; i < size(); ++i)
            {
                if (*(data_->get(i)) == value)
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
            assert(n < size());
            assert(!empty());
            return *(data_->get(n));
        }

        template < typename Allocator > iterator erase(Allocator& allocator, iterator it)
        {
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);
            std::allocator_traits< decltype(alloc) >::destroy(alloc, data_->get(it.index_));
            move(alloc, data_->get(it.index_), data_->get(it.index_ + 1), data_->size - it.index_ - 1);
            --data_->size;
            if (data_->size == 0)
            {
                clear(allocator);
            }

            return iterator(this, it.index_);
        }

        template < typename Allocator > void clear(Allocator& allocator)
        {
            if (data_)
            {
                auto alloc = std::allocator_traits< Allocator >::rebind_alloc< unsigned char >(allocator);
                destroy(alloc, data_->get(), data_->size);
                alloc.deallocate((unsigned char*)data_, sizeof(vector_data) + sizeof(T) * data_->capacity);
                data_ = nullptr;
            }
        }

        size_type size() const 
        { 
            return data_ ? data_->size : 0;
        }

        size_type capacity() const
        {
            return data_ ? data_->capacity : 0;
        }

        bool empty() const { return size() == 0; }

        iterator begin() { return iterator(this, 0); }
        iterator begin() const { return iterator(const_cast<vector_base< T >*>(this), 0); }
        iterator end() { return iterator(this, size()); }
        iterator end() const { return iterator(const_cast<vector_base< T >*>(this), size()); }

    private:
        template < typename Allocator > void grow(Allocator& allocator, size_type nsize)
        {
            size_type ocapacity = capacity();
            if (ocapacity < nsize)
            {
                auto alloc = std::allocator_traits< Allocator >::rebind_alloc< unsigned char >(allocator);

                size_type ncapacity = size_type(ocapacity + nsize) * 3/2;
                vector_data* data = (vector_data*)alloc.allocate(sizeof(vector_data) + sizeof(T) * ncapacity);
                data->capacity = ncapacity;
                data->size = 0;

                if (data_)
                {
                    move_uninitialized(alloc, data->get(), data_->get(), data_->size);
                    data->size = data_->size;
                    alloc.deallocate((unsigned char*)data_, sizeof(vector_data) + sizeof(T) * ocapacity); // TODO: deallocate at the end
                }

                data_ = data;
            }
        }

        vector_data* data_;
    };
}
