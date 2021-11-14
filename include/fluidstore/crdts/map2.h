#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/set2.h>

#include <stdexcept>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag, template <typename> typename Hook = hook_none >
    class map2;

    template < typename Key, typename Value, typename Allocator, template <typename> typename Hook >
    class map2< Key, Value, Allocator, tag_delta, Hook >
        : public dot_kernel< Key, Value, Allocator, map2< Key, Value, Allocator, tag_delta, Hook >, tag_delta >
    {
    public:
        using allocator_type = Allocator;

        map2(allocator_type& allocator)
            : allocator_(allocator)
        {}

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    private:
        allocator_type& allocator_;
    };

    template < typename Key, typename Value, typename Allocator, template <typename> typename Hook >
    class map2< Key, Value, Allocator, tag_state, Hook >
        : public dot_kernel< Key, void, Allocator, map2< Key, Value, Allocator, tag_state, Hook >, tag_state >
        , public Hook < map2 < Key, Value, Allocator, tag_delta > >
    {
    public:
        using allocator_type = Allocator;
        using dot_kernel_type = dot_kernel< Key, void, Allocator, map2< Key, Value, Allocator, tag_state, Hook >, tag_state >;
        using iterator = typename dot_kernel_type::iterator;

        template < typename AllocatorT > struct rebind
        {
            using other = map2< Key, typename Value::template rebind< AllocatorT, Hook >::other, AllocatorT, tag_state, Hook >;
        };

        map2(allocator_type& allocator)
            : allocator_(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            
            map2< Key, Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot, value);          
                        
            insert_context context;
            merge(delta, context);
            commit_delta(delta);
            return { context.result.first, context.result.second };
        }

        auto& operator[](const Key& key)
        {
            if constexpr (std::uses_allocator_v< Value, Allocator >)
            {
                // TODO: this Value should not be tracked, it is temporary.
                auto pairb = insert(key, Value(this->get_allocator()));
                return (*pairb.first).second;
            }
            else
            {
                auto pairb = insert(key, Value());
                return (*pairb.first).second;
            }
        }

        auto& at(const Key& key)
        {
            auto it = find(key);
            if (it != end())
            {
                return (*it).second;
            }
            else
            {
                throw std::out_of_range(__FUNCTION__);
            }
        }

        const auto& at(const Key& key) const
        {
            return const_cast<decltype(*this)>(this)->at(key);
        }

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    private:
        allocator_type& allocator_;
    };
}