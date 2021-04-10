#pragma once

#include <type_traits>
#include <memory>

namespace crdt::flat
{
    template < typename Allocator, typename T, typename SizeType > void destroy(Allocator& allocator, T* source, SizeType count)
    {
        if constexpr (!std::is_trivially_destructible_v< T >)
        {
            for (SizeType i = 0; i < count; ++i)
            {
                std::allocator_traits< Allocator >::destroy(allocator, &source[i]);
            }
        }
    }

    template < typename Allocator, typename T, typename SizeType > void move_uninitialized(Allocator& allocator, T* destination, T* source, SizeType count)
    {
        if constexpr (std::is_trivially_copyable_v< T > || std::is_trivially_move_constructible_v< T >)
        {
            memcpy(destination, source, count * sizeof(T));
        } 
        else if constexpr (std::is_move_constructible_v< T >)
        {
            std::uninitialized_move(source, source + count, destination);
        }
        else
        {
            static_assert(false);
        }

        destroy(allocator, source, count);
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
       1 // std::is_move_assignable_v< T >
    >::type* = 0 > void move_construct(Allocator& allocator, T* destination, T* source, SizeType count)
    {
        for (SizeType i = 0; i < count; ++i)
        {
            std::allocator_traits< Allocator >::destroy(allocator, &destination[i]);
            std::allocator_traits< Allocator >::construct(allocator, &destination[i], std::move(source[i]));
        }
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        std::is_trivially_copyable_v< T > && 
        std::is_trivially_destructible_v< T >
    >::type* = 0 > void move(Allocator&, T* destination, T* source, SizeType count)
    {
        memmove(destination, source, count * sizeof(T));
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        !std::is_trivially_copyable_v< T > &&
        std::is_move_constructible_v< T >
    >::type* = 0 > void move(Allocator& allocator, T* destination, T* source, SizeType count)
    {
        if (destination < source)
        {
            ptrdiff_t diff = source - destination;
            if (diff >= count)
            {
                move_uninitialized(allocator, destination, source, count);
            }
            else
            {
                move_uninitialized(allocator, destination, source, diff);
                move_construct(allocator, destination + diff, source + diff, count - diff);
                destroy(allocator, source + count - diff, diff);
            }
        }
        else
        {
            ptrdiff_t diff = destination - source;
            if (diff >= count)
            {
                move_uninitialized(allocator, destination, source, count);
            }
            else
            {
                move_uninitialized(allocator, destination + count - diff, source + count - diff, diff);
                move_construct(allocator, destination, source, count - diff);
                destroy(allocator, source, diff);
            }
        }
    }
}