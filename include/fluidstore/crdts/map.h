#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/default_hook.h>

#include <stdexcept>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag, typename Hook, typename Delta > class map_base
        : public Hook::template hook< Allocator, Delta, map_base< Key, Value, Allocator, Tag, Hook, Delta > >
        , public dot_kernel< Key, Value, Allocator, map_base< Key, Value, Allocator, Tag, Hook, Delta >, Tag >
    {
        typedef map_base< Key, Value, Allocator, Tag, Hook, Delta > map_base_type;
        typedef dot_kernel< Key, Value, Allocator, map_base_type, Tag > dot_kernel_type;
    
    public:
        using allocator_type = Allocator;
        using hook_type = typename Hook::template hook< allocator_type, Delta, map_base_type >;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = map_base< Key, typename Value::template rebind< AllocatorT, HookT >::other, AllocatorT, Tag, HookT, Delta >;
        };

        struct delta_extractor
        {
            template < typename Delta > void apply(map_base_type& instance, Delta& delta) 
            {
                for (const auto& [rkey, rvalue] : delta)
                {
                    auto lvalue_it = instance.find(rkey);
                    if (lvalue_it != instance.end())
                    {
                        auto& lvalue = (*lvalue_it).second;
                        std::remove_reference_t< decltype(lvalue) >::delta_extractor extractor;
                        rvalue.merge(lvalue.extract_delta());
                        extractor.apply(lvalue, rvalue);
                    }
                }
            }
        };

        map_base(allocator_type& allocator)
            : hook_type(allocator)
        {}

        map_base(map_base_type&& other) = default;

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            auto delta = mutable_delta(deltaallocator);

            insert(delta, key, value);

            // TODO:
            // When this is insert of one element, merge should change internal state only when an insert operation would.
            // But we already got id. So the id would have to be returned, otherwise dot sequence will not collapse.
            
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
            return const_cast< map_base_type* >(this)->at(key);
        }

    private:
        template < typename Delta, typename ValueT > void insert(Delta& delta, const Key& key, const ValueT& value)
        {
            auto dot = this->get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot, value);
        }
    };

    template < typename Key, typename Value, typename Allocator, typename Tag, typename Hook > class map_base< Key, Value, Allocator, Tag, Hook, void >
        : public Hook::template hook< Allocator, void, map_base< Key, Value, Allocator, Tag, Hook, void > >
        , public dot_kernel< Key, Value, Allocator, map_base< Key, Value, Allocator, Tag, Hook, void >, Tag >
    {
        typedef dot_kernel< Key, Value, Allocator, map_base< Key, Value, Allocator, Tag, Hook, void >, Tag > dot_kernel_type;
        typedef map_base< Key, Value, Allocator, Tag, Hook, void > map_base_type;
        typedef typename Hook::template hook< Allocator, void, map_base_type > hook_type;

    public:
        using allocator_type = Allocator;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = map_base< Key, typename Value::template rebind< AllocatorT, HookT >::other, AllocatorT, Tag, HookT, void >;
        };

        map_base(allocator_type allocator)
            : hook_type(allocator)
        {}

        map_base(map_base_type&& other) = default;
    };

    template < typename Key, typename Value, typename Allocator, typename Hook = default_state_hook,
        typename Delta = map_base <
            Key,
            typename Value::delta_type,
            Allocator, 
            tag_delta, 
            default_delta_hook, 
            void 
        >
    > class map
        : public map_base< Key, typename Value::template rebind< Allocator, Hook >::other, Allocator, tag_state, Hook, Delta >
    {
        using map_base_type = map_base< Key, typename Value::template rebind< Allocator, Hook >::other, Allocator, tag_state, Hook, Delta >;

    public:
        using allocator_type = Allocator;
        using hook_type = Hook;
        using delta_type = Delta;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            typedef map< Key, typename Value::template rebind< AllocatorT, HookT >::other, AllocatorT, HookT, Delta > other;
        };

        map(allocator_type& allocator)
            : map_base_type(allocator)
        {}
    };
}