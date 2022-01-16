#pragma once

// Allocator is based on https://howardhinnant.github.io/allocator_boilerplate.html

#include <array>
#include <new>
#include <memory>

namespace memory
{
    template < typename Allocator > struct is_stateless_allocator : std::false_type {};
    template < typename T > struct is_stateless_allocator< std::allocator < T > > : std::true_type {};

    template < typename Allocator, bool = is_stateless_allocator< Allocator >::value > class allocator_storage;

    template < typename Allocator > class allocator_storage < Allocator, true >
    {
    public:
        allocator_storage() {}
        template < typename AllocatorT > allocator_storage(AllocatorT&&) {}

        Allocator get_allocator() { return Allocator(); }

        allocator_storage< Allocator, true >& operator = (const allocator_storage< Allocator, true >&) { return *this; }
        bool operator == (const allocator_storage< Allocator, true >&) const { return true; }
    };

    template < typename Allocator > class allocator_storage < Allocator, false >
    {
    public:
        template < typename AllocatorT > allocator_storage(AllocatorT&& allocator)
            : allocator_(typename std::allocator_traits< Allocator >::rebind_alloc< typename Allocator::value_type >(std::forward< AllocatorT >(allocator)))
        {}

        Allocator& get_allocator() { return allocator_; }

        allocator_storage< Allocator, true >& operator = (const allocator_storage< Allocator, true >& other)
        {
            allocator_ = other.allocator_;
            return *this;
        }

        bool operator == (const allocator_storage< Allocator, true >& other) const { return allocator_ == other.allocator_; }

    private:
        Allocator allocator_;
    };

    // TODO: This should be lazy for some usecases. 
    template< typename Allocator = std::allocator< void > > class dynamic_buffer
        : allocator_storage< Allocator >
    {
        dynamic_buffer(const dynamic_buffer< Allocator >&) = delete;
        dynamic_buffer< Allocator >& operator = (const dynamic_buffer< Allocator >&) = delete;

        dynamic_buffer(dynamic_buffer< Allocator >&&) = delete;
        dynamic_buffer< Allocator >& operator = (dynamic_buffer< Allocator >&&) = delete;

    public:
        dynamic_buffer(size_t size)
            : base_()
            , current_()
            , allocated_()
            , size_(size)
        {
            base_ = current_ = typename std::allocator_traits< Allocator >::template rebind_alloc< uint8_t >(get_allocator()).allocate(size_);
        }

        dynamic_buffer(size_t size, Allocator& allocator)
            : allocator_storage< typename std::allocator_traits< Allocator >::template rebind_alloc< uint8_t > >(allocator)
            , base_()
            , current_()
            , allocated_()
            , size_(size)
        {
            base_ = current_ = typename std::allocator_traits< Allocator >::template rebind_alloc< uint8_t >(get_allocator()).allocate(size_);
        }

        ~dynamic_buffer()
        {
            typename std::allocator_traits< Allocator >::rebind_alloc< uint8_t >(get_allocator()).deallocate(base_, size_);
        }

        template < typename T > T* allocate(std::size_t n)
        {
            n *= sizeof(T);
            unsigned char* ptr = (unsigned char*)(uintptr_t(current_ + alignof(T) - 1) & ~(alignof(T) - 1));
            if (ptr + n <= base_ + size_)
            {
                current_ = ptr + n;
                allocated_ += n;
                return reinterpret_cast<T*>(ptr);
            }
            else
            {
                return nullptr;
            }
        }

        template < typename T > bool deallocate(T* p, std::size_t n)
        {
            n *= sizeof(T);
            auto ptr = reinterpret_cast<unsigned char*>(p);
            if (ptr >= base_ && ptr + n <= base_ + size_)
            {
                allocated_ -= n;
                if (allocated_ == 0)
                {
                    current_ = base_;
                }

                return true;
            }

            return false;
        }

        std::size_t get_allocated() const { return allocated_; }

    private:
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

        template < typename T > T* allocate(std::size_t n)
        {
            n *= sizeof(T);
            unsigned char* ptr = (unsigned char*)(uintptr_t(current_ + alignof(T) - 1) & ~(alignof(T) - 1));
            if (ptr + n <= base_.data() + Size)
            {
                current_ = ptr + n;
                allocated_ += n;
                return reinterpret_cast<T*>(ptr);
            }
            else
            {
                return nullptr;
            }
        }

        template < typename T > bool deallocate(T* p, std::size_t n)
        {
            n *= sizeof(T);
            auto ptr = reinterpret_cast<unsigned char*>(p);
            if (ptr >= base_.data() && ptr + n <= base_.data() + Size)
            {
                allocated_ -= n;
                if (allocated_ == 0)
                {
                    current_ = base_.data();
                }

                return true;
            }

            return false;
        }

        std::size_t get_allocated() const { return allocated_; }

    private:
        std::array< uint8_t, Size > base_;
        uint8_t* current_;
        std::size_t allocated_;
    };

    template< typename Allocator = std::allocator< uint8_t > > class stats_buffer
        : allocator_storage< Allocator >
    {
        stats_buffer(const stats_buffer< Allocator >&) = delete;
        stats_buffer< Allocator >& operator = (const stats_buffer< Allocator >&) = delete;

        stats_buffer(stats_buffer< Allocator >&&) = delete;
        stats_buffer< Allocator >& operator = (stats_buffer< Allocator >&&) = delete;

    public:
        stats_buffer()
            : allocated_()
        {}

        template< typename AllocatorT > stats_buffer(AllocatorT&& allocator)
            : allocator_storage< Allocator >(std::forward< AllocatorT >(allocator))
            , allocated_()
        {}

        ~stats_buffer() {}

        template < typename T > T* allocate(std::size_t n)
        {
            T* p = typename std::allocator_traits< Allocator >::template rebind_alloc< T >(get_allocator()).allocate(n);
            allocated_ += n * sizeof(T);
            return p;
        }

        template < typename T > bool deallocate(T* p, std::size_t n)
        {
            allocated_ -= n * sizeof(T);
            typename std::allocator_traits< Allocator >::template rebind_alloc< T >(get_allocator()).deallocate(p, n);
            return true;
        }

        std::size_t get_allocated() const { return allocated_; }

    private:
        std::size_t allocated_;
    };

    template < typename T > class buffer_allocator_throw
    {
    public:
        using value_type = T;

        buffer_allocator_throw() = default;
        template < typename U > buffer_allocator_throw(U&&) {}

        T* allocate(std::size_t) { throw std::bad_alloc(); }
        bool deallocate(T*, std::size_t) { throw std::bad_alloc(); }

        template < typename U > bool operator == (const buffer_allocator_throw< U >&) const { return true; }
    };

    template < typename T > struct is_stateless_allocator< buffer_allocator_throw< T > > : std::true_type {};
        
    template < typename T, typename Buffer, typename FallbackAllocator = buffer_allocator_throw< T > > class buffer_allocator
        : allocator_storage< FallbackAllocator >
    {
        template < typename U, typename BufferU, typename FallbackAllocatorU > friend class buffer_allocator;
                
    public:
        using value_type = T;

        template< typename U > struct rebind
        {
            using other = buffer_allocator < U, Buffer, typename std::allocator_traits< FallbackAllocator >::template rebind_alloc< U > > ;
        };

        buffer_allocator(Buffer& buffer) noexcept
            : buffer_(&buffer)
        {}
        
        template < typename FallbackAlloc > buffer_allocator(Buffer& buffer, FallbackAlloc&& fallback) noexcept
            : allocator_storage< FallbackAllocator >(fallback)
            , buffer_(&buffer)
        {}

        template < typename U, typename FallbackAllocatorU > buffer_allocator(const buffer_allocator< U, Buffer, FallbackAllocatorU >& other) noexcept
            : allocator_storage< FallbackAllocator >(other)
            , buffer_(other.buffer_)
        {}
                                
        buffer_allocator< T, Buffer >& operator = (const buffer_allocator< T, Buffer >& other)
        {
            buffer_ = other.buffer_;
            static_cast<allocator_storage< FallbackAllocator >&>(*this) = other;
            return *this;
        }

        value_type* allocate(std::size_t n)
        {
            auto ptr = buffer_->template allocate< value_type >(n);
            if (!ptr)
            {                
                return get_allocator().allocate(n);
            }
            return ptr;
        }

        void deallocate(value_type* p, std::size_t n) noexcept
        {
            if (!buffer_->deallocate(p, n))
            {
                get_allocator().deallocate(p, n);
            }
        }
        
        template < typename U, typename FallbackU > bool operator == (const buffer_allocator< U, Buffer, FallbackU >& other) const
        {
            return buffer_ == other.buffer_ && 
                static_cast< const allocator_storage< FallbackAllocator >& >(*this) == other;
        }
                
    private:
        Buffer* buffer_;
    };
}
