#pragma once

#include <type_traits>
#include <memory>

namespace crdt::flat
{
    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        std::is_trivially_destructible_v< T >
    >::type* = 0 > void destroy(Allocator&, T* source, SizeType count)
    {}

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        !std::is_trivially_destructible_v< T >
    >::type* = 0 > void destroy(Allocator&, T* source, SizeType count)
    {
        for (SizeType i = 0; i < count; ++i)
        {
            source[i].~T();
        }
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        std::is_trivially_copyable_v< T > && 
        std::is_trivially_destructible_v< T >
    >::type* = 0 > void move_uninitialized(Allocator& allocator, T* destination, T* source, SizeType count)
    {
        memcpy(destination, source, count * sizeof(T));
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        !std::is_trivially_copyable_v< T >&&
        std::is_move_constructible_v< T >
    >::type* = 0 > void move_uninitialized(Allocator& allocator, T* destination, T* source, SizeType count)
    {
        std::uninitialized_move(source, source + count, destination);
        destroy(allocator, source, count);
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
            if (diff >= ptrdiff_t(count * sizeof(T)))
            {
                move_uninitialized(allocator, destination, source, count);
                destroy(allocator, source, count);
            }
            else
            {
                SizeType uninitialized_count = diff / sizeof(T);
                move_uninitialized(allocator, destination, source, uninitialized_count);
                move_assign(allocator, destination + uninitialized_count, source + uninitialized_count, count - uninitialized_count);
                destroy(allocator, source + count - uninitialized_count, uninitialized_count);
            }
        }
        else
        {
            ptrdiff_t diff = destination - source;
            if (diff >= ptrdiff_t(count * sizeof(T)))
            {
                move_uninitialized(allocator, destination, source, count);
            }
            else
            {
                SizeType uninitialized_count = diff / sizeof(T);
                move_uninitialized(allocator, destination + count - uninitialized_count, source + count - uninitialized_count, uninitialized_count);
                move_assign(allocator, destination, source, count - uninitialized_count);
                destroy(allocator, source, uninitialized_count);
            }
        }
    }

    template < typename Allocator, typename T, typename SizeType, typename std::enable_if<
        std::is_move_assignable_v< T >
    >::type* = 0 > void move_assign(Allocator&, T* destination, T* source, SizeType count)
    {
        for (SizeType i = 0; i < count; ++i)
        {
            destination[i] = std::move(source[i]);
        }
    }
}