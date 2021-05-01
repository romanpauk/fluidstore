#pragma once

#include <fluidstore/flat/memory.h>

#include <iterator>

namespace crdt::flat
{
    template < typename T, typename SizeType = uint32_t > class vector_base
    {
    public:
        using size_type = SizeType;
        using value_type = T;

    private:
        struct vector_data
        {
            size_type size;
            size_type capacity;
            alignas(T) unsigned char buffer[1];

            T* get(size_type n = 0) { return (T*)buffer + n; }

            static size_t get_memory_size(size_type capacity)
            {
                return sizeof(vector_data) + sizeof(T) * capacity - sizeof(vector_data::buffer);
            }
        };

    public:
        struct iterator : public std::iterator< std::random_access_iterator_tag, T, size_type >
        {
            friend class vector_base< T, SizeType >;

            using typename std::iterator< std::random_access_iterator_tag, T, size_type >::difference_type;

            iterator()
                : p_()
                , index_()
            {}

            iterator(vector_base< T, SizeType >* p, size_type index)
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
            vector_base< T, SizeType >* p_;
            size_type index_;
        };

        // TODO:
        using const_iterator = iterator;

        vector_base()
            : data_()
        {}

        vector_base(vector_base< T, SizeType >&& other)
            : data_(other.data_)
        {
            other.data_ = nullptr;
        }

        vector_base(const vector_base< T, SizeType >&) = delete;
        vector_base< T >& operator = (const vector_base< T >&) = delete;
    
    #if defined(_DEBUG)
        ~vector_base()
        {
            assert(!data_);
        }
    #else
        ~vector_base() = default;
    #endif

        template < typename Allocator > void push_back(Allocator& allocator, const T& value)
        {
            emplace(allocator, end(), value);
        }

        template < typename Allocator, typename... Args > iterator emplace(Allocator& allocator, iterator position, Args&&... args)
        {
            auto alloc = std::allocator_traits< Allocator >::rebind_alloc< T >(allocator);

            size_type index = position.index_;

            if (std::numeric_limits< size_type >::max() - 1 < size())
            {
                throw std::runtime_error("overflow");
            }

            reserve(allocator, size() + 1); // TODO: this is a bit stupid, we copy first everything and than move it again
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
                alloc.deallocate((unsigned char*)data_, vector_data::get_memory_size(data_->capacity));
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

        constexpr size_type max_size() const
        {
            return std::numeric_limits< size_type >::max();
        }

        bool empty() const { return size() == 0; }

        iterator begin() { return iterator(this, 0); }
        iterator begin() const { return iterator(const_cast<vector_base< T >*>(this), 0); }
        iterator end() { return iterator(this, size()); }
        iterator end() const { return iterator(const_cast<vector_base< T >*>(this), size()); }

        T& back()
        {
            assert(!empty());
            return *data_->get(size() - 1);
        }

        const T& back() const
        {
            return const_cast<vector_base< T, SizeType >& >(*this).back();
        }

        template < typename Allocator > void reserve(Allocator& allocator, size_type nsize)
        {
            size_type ocapacity = capacity();
            if (ocapacity < nsize)
            {
                auto alloc = std::allocator_traits< Allocator >::rebind_alloc< unsigned char >(allocator);

                size_type ncapacity = (ocapacity + nsize);
                if (ncapacity < ocapacity)
                {
                    throw std::runtime_error("overflow");
                }

                if ((ncapacity * 3 / 2) > ncapacity)
                {
                    ncapacity *= 3 / 2;
                }

                vector_data* data = (vector_data*)alloc.allocate(vector_data::get_memory_size(ncapacity));
                data->capacity = ncapacity;
                data->size = 0;

                if (data_)
                {
                    move_uninitialized(alloc, data->get(), data_->get(), data_->size);
                    data->size = data_->size;
                    alloc.deallocate((unsigned char*)data_, vector_data::get_memory_size(ocapacity)); // TODO: deallocate at the end
                }

                data_ = data;
            }
        }

        template < typename Allocator > void assign(Allocator& allocator, iterator begin, iterator end)
        {
            clear(allocator);
            auto size = std::distance(begin, end);
            reserve(allocator, size);
            copy_uninitialized(allocator, data_->get(), &*begin, size);
            data_->size = size;
        }

        template < typename Ty > void update(iterator it, Ty&& value)
        {
            assert(!empty());
            *data_->get(it.index_) = std::forward< Ty >(value);
        }

    private:
        vector_data* data_;
    };

    template < typename T, typename Allocator, typename SizeType = uint32_t > class vector : private vector_base< T, SizeType >
    {
        using vector_base_type = vector_base< T, SizeType >;

    public:
        using typename vector_base_type::iterator;
        using typename vector_base_type::const_iterator;
        using typename vector_base_type::value_type;

        vector(Allocator& allocator)
            : allocator_(allocator)
        {}

        vector(vector< T, Allocator, SizeType >&& other) = default;

        ~vector()
        {
            clear();
        }

        void clear()
        {
            return vector_base_type::clear(allocator_);
        }

        void push_back(const T& value)
        {
            vector_base_type::push_back(allocator_, value);
        }

        using vector_base_type::begin;
        using vector_base_type::end;
        using vector_base_type::size;
        using vector_base_type::empty;

    private:
        Allocator& allocator_;
    };
}
