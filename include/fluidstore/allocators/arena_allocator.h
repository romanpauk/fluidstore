#pragma once

#include <array>

namespace crdt
{
    template < typename T, typename Allocator > class arena_allocator;

    class arena_base
    {
        template < typename T, typename Allocator > friend class arena_allocator;

    public:
        template < size_t Size > arena_base(std::array< unsigned char, Size >& arena)
            : base_(arena.data())
            , current_(arena.data())
            , end_(arena.data() + arena.size())
        {}

        bool operator == (const arena_base& arena) { return base_ == arena.base_; }

        arena_base& operator = (const arena_base& other)
        {
            base_ = other.base_;
            current_ = other.current_;
            end_ = other.end_;
            return *this;
        }

    private:
        unsigned char* base_;
        unsigned char* current_;
        unsigned char* end_;
    };

    template< size_t N > class arena : public arena_base
    {
    public:
        arena()
            : arena_base(buffer_)
        {}

        std::array< unsigned char, N > buffer_;
    };

    template < typename T = void, typename Allocator = std::allocator< T > > class arena_allocator
        : public Allocator
    {
    public:
        using value_type = T;

        template< typename U > struct rebind
        {
            typedef arena_allocator< U,
                typename std::allocator_traits< Allocator >::template rebind_alloc< U >
            > other;
        };

        template < typename... Args > arena_allocator(arena_base& arena, Args&&... args) noexcept
            : arena_(arena)
            , Allocator(std::forward< Args >(args)...)
        {}

        template < typename T, typename Allocator > friend class arena_allocator;

        template < typename U, typename AllocatorU > arena_allocator(const arena_allocator< U, AllocatorU >& alloc) noexcept
            : arena_(alloc.arena_)
            , Allocator(alloc)
        {}

        arena_allocator< T, Allocator >& operator = (const arena_allocator< T, Allocator >& other)
        {
            arena_ = other.arena_;
            return *this;
        }

        value_type* allocate(std::size_t n)
        {
            unsigned char* ptr = (unsigned char*)(uintptr_t(arena_.current_ + alignof(value_type) - 1) & ~(alignof(value_type) - 1));

            if (ptr + n * sizeof(value_type) < arena_.end_)
            {
                value_type* p = reinterpret_cast<value_type*>(ptr);
                ptr += n * sizeof(value_type);
                arena_.current_ = ptr;
                return p;
            }
            else
            {
            #ifdef _DEBUG
                assert(false);
            #endif
               return Allocator::allocate(n);
            }
        }

        void deallocate(value_type* p, std::size_t size) noexcept
        {
            auto ptr = reinterpret_cast<unsigned char*>(p);
            if (ptr >= arena_.base_ && ptr < arena_.end_)
            {
                // This is from arena
            }
            else
            {
                Allocator::deallocate(p, size);
            }
        }

    private:
        arena_base& arena_;
    };

    template < typename T, typename U, typename Allocator > bool operator == (arena_allocator< T, Allocator > const& lhs, arena_allocator< U, Allocator > const& rhs) noexcept
    {
        return lhs.arena_ == rhs.arena_;
    }

    template < typename T, typename U, typename Allocator > bool operator != (arena_allocator< T, Allocator > const& x, arena_allocator< U, Allocator > const& y) noexcept
    {
        return !(x == y);
    }
}
