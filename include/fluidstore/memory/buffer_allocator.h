#pragma once

// Allocator is based on https://howardhinnant.github.io/allocator_boilerplate.html

#include <array>
#include <new>

namespace memory
{
    template< typename Allocator = std::allocator< void > > class dynamic_buffer
    {
        dynamic_buffer(const dynamic_buffer< Allocator >&) = delete;
        dynamic_buffer< Allocator >& operator = (const dynamic_buffer< Allocator >&) = delete;

        dynamic_buffer(dynamic_buffer< Allocator >&&) = delete;
        dynamic_buffer< Allocator >& operator = (dynamic_buffer< Allocator >&&) = delete;

    public:
        using value_type = typename Allocator::value_type;
        using allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< uint8_t >;
                
        dynamic_buffer(size_t size)
            : allocator_()
            , base_()
            , current_()
            , allocated_()
            , size_(size)
        {
            base_ = current_ = allocator_.allocate(size_);
        }

        dynamic_buffer(size_t size, Allocator& allocator)
            : allocator_(allocator)
            , base_()
            , current_()
            , allocated_()
            , size_(size)
        {
            base_ = current_ = allocator_.allocate(size_);
        }

        ~dynamic_buffer()
        {
            allocator_.deallocate(base_, size_);
        }

        void* allocate(std::size_t n, std::size_t alignment)
        {            
            unsigned char* ptr = (unsigned char*)(uintptr_t(current_ + alignment - 1) & ~(alignment - 1));
            if (ptr + n < base_ + size_)
            {
                current_ = ptr + n;
                allocated_ += n;
                return ptr;
            }
            else
            {
                return nullptr;
            }
        }

        void deallocate(void* p, std::size_t n)
        {
            auto ptr = reinterpret_cast<unsigned char*>(p);
            assert(ptr >= base_ && ptr + n <= base_ + size_);
        
            allocated_ -= n;
            if (allocated_ == 0)
            {
                current_ = base_;
            }
        }

        std::size_t get_allocated() const { return allocated_; }

    private:
        allocator_type allocator_;

        uint8_t* base_;
        uint8_t* current_;
        std::size_t allocated_;
        std::size_t size_;
    };

    template< size_t Size > class static_buffer
    {
        static_buffer(const static_buffer< Size >&) = delete;
        static_buffer< Size >& operator = (const static_buffer< Size >&) = delete;

        static_buffer(static_buffer< Size >&&) = delete;
        static_buffer< Size >& operator = (static_buffer< Size >&&) = delete;

    public:
        static_buffer()
            : current_()
            , allocated_()
        {
            current_ = base_.data();
        }

        ~static_buffer()
        {}

        void* allocate(std::size_t n, std::size_t alignment)
        {
            unsigned char* ptr = (unsigned char*)(uintptr_t(current_ + alignment - 1) & ~(alignment - 1));
            if (ptr + n < base_.data() + Size)
            {                
                current_ = ptr + n;
                allocated_ += n;
                return ptr;
            }
            else
            {
                return nullptr;
            }
        }

        void deallocate(void* p, std::size_t n)
        {
            auto ptr = reinterpret_cast<unsigned char*>(p);
            assert(ptr >= base_.data() && ptr + n <= base_.data() + Size);

            allocated_ -= n;
            if (allocated_ == 0)
            {
                current_ = base_.data();
            }
        }

        std::size_t get_allocated() const { return allocated_; }

    private:
        std::array< uint8_t, Size > base_;
        uint8_t* current_;
        std::size_t allocated_;
    };

    template < typename T, typename Buffer > class buffer_allocator
    {
        template < typename U, typename BufferU > friend class buffer_allocator;

    public:
        using value_type = T;

        template< typename U > struct rebind
        {
            using other = buffer_allocator < U, Buffer > ;
        };

        buffer_allocator(Buffer& buffer) noexcept
            : buffer_(&buffer)
        {}
        
        template < typename U > buffer_allocator(const buffer_allocator< U, Buffer >& other) noexcept 
            : buffer_(other.buffer_)
        {}                       
                
        buffer_allocator< T, Buffer >& operator = (const buffer_allocator< T, Buffer >& other)
        {
            buffer_ = other.buffer_;
            return *this;
        }

        value_type* allocate(std::size_t n)
        {
            auto ptr = reinterpret_cast< value_type* >(buffer_->allocate(n * sizeof(value_type), alignof(value_type)));
            if (!ptr)
            {
                throw std::bad_alloc();
            }
            return ptr;
        }

        void deallocate(value_type* p, std::size_t n) noexcept
        {
            buffer_->deallocate(p, n * sizeof(value_type));
        }
        
    //private:
        Buffer* buffer_;
    };

    template < typename T, typename U, typename Buffer > bool operator == (buffer_allocator< T, Buffer > const& lhs, buffer_allocator< U, Buffer > const& rhs) noexcept
    {
        return lhs.buffer_ == rhs.buffer_;
    }

    template < typename T, typename U, typename Buffer > bool operator != (buffer_allocator< T, Buffer > const& x, buffer_allocator< U, Buffer > const& y) noexcept
    {
        return !(x == y);
    }
}
