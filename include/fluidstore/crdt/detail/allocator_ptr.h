#pragma once

#include <memory>

namespace crdt
{
    template < typename T > struct allocator_ptr_base
    {
        allocator_ptr_base()
            : ptr_()
        {}

        allocator_ptr_base(allocator_ptr_base< T >&& other)
            : ptr_()
        {
            std::swap(ptr_, other.ptr_);
        }

        allocator_ptr_base< T >& operator = (allocator_ptr_base< T >&& other)
        {
            assert(!ptr_);
            std::swap(ptr_, other.ptr_);
            return *this;
        }

        ~allocator_ptr_base() = default;

        template < typename Allocator, typename... Args > void emplace(Allocator& allocator, Args&&... args)
        {
            assert(!ptr_);

        #if defined(__clang__)
            typename Allocator::template rebind< T >::other alloc(allocator);
        #else
            auto alloc = std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
        #endif

            reset(alloc);

            ptr_ = alloc.allocate(1);
            std::allocator_traits< decltype(alloc) >::construct(alloc, ptr_, std::forward< Args >(args)...);
        }

        T& operator*() { assert(ptr_); return *ptr_; }
        const T& operator*() const { assert(ptr); return *ptr_; }
        T* operator->() { assert(ptr_); return ptr_; }
        const T* operator->() const { assert(ptr_); return ptr_; }

        bool operator !() const { return ptr_ == nullptr; }
        operator bool() const { return ptr_ != nullptr; }

        template < typename Allocator > void reset(Allocator& allocator)
        {
            if (ptr_)
            {
            #if defined(__clang__)
                typename Allocator::template rebind< T >::other alloc(allocator);
            #else
                auto alloc = std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator);
            #endif

                std::allocator_traits< decltype(alloc) >::destroy(alloc, ptr_);
                std::allocator_traits< decltype(alloc) >::deallocate(alloc, ptr_, 1);
                ptr_ = nullptr;
            }
        }

    private:
        T* ptr_;
    };

    template < typename T, typename Allocator > struct allocator_ptr
        : public allocator_ptr_base< T >
    {
        allocator_ptr(Allocator& allocator)
            : allocator_(allocator)
        {}

        allocator_ptr(allocator_ptr< T, Allocator >&& other) = default;
        ~allocator_ptr() { reset(allocator_); }

        template < typename... Args > void emplace(Args&&... args)
        {
            assert(!ptr_);
            reset(allocator_);
            emplace(allocator_, std::forward< Args >(args)...);
        }

        void reset()
        {
            reset(allocator_);
        }

    private:
        Allocator& allocator_;
    };
}