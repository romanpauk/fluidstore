#pragma once

#include <memory>

namespace crdt
{
    template < typename T, typename Allocator > struct allocator_ptr
    {
        allocator_ptr(Allocator& allocator)
            : allocator_(allocator)
            , ptr_()
        {}

        ~allocator_ptr() { reset(); }

        template < typename... Args > void emplace(Args&&... args)
        {
            assert(!ptr_);
            reset();

            auto allocator = std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator_);
            ptr_ = allocator.allocate(1);
            std::allocator_traits< decltype(allocator) >::construct(allocator, ptr_, std::forward< Args >(args)...);
        }

        T& operator*() { assert(ptr_); return *ptr_; }
        const T& operator*() const { assert(ptr); return *ptr_; }
        T* operator->() { assert(ptr_); return ptr_; }
        const T* operator->() const { assert(ptr_); return ptr_; }

        bool operator !() const { return ptr_ == nullptr; }
        operator bool() const { return ptr_ != nullptr; }

        void reset()
        {
            if (ptr_)
            {
                auto allocator = std::allocator_traits< Allocator >::template rebind_alloc< T >(allocator_);
                std::allocator_traits< decltype(allocator) >::destroy(allocator, ptr_);
                std::allocator_traits< decltype(allocator) >::deallocate(allocator, ptr_, 1);
                ptr_ = nullptr;
            }
        }

    private:
        Allocator& allocator_;
        T* ptr_;
    };
}