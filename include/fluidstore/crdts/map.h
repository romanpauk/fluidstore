#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/set.h>

#include <stdexcept>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag = crdt::tag_state, template <typename,typename,typename> typename Hook = hook_default >
    class map;

    template < typename Key, typename Value, typename Allocator, template <typename, typename, typename> typename Hook >
    class map< Key, Value, Allocator, tag_delta, Hook >
        : public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_delta, Hook >, tag_delta >
        , public hook_default< void, Allocator, void >
    {
    public:
        using allocator_type = Allocator;
        using tag_type = tag_delta;

        template < typename AllocatorT, typename TagT = tag_delta, template <typename,typename,typename> typename HookT = Hook > struct rebind
        {
            using other = map< Key, typename Value::template rebind< AllocatorT, TagT, HookT >::other, AllocatorT, TagT, HookT >;
        };

        map(allocator_type& allocator)
            : hook_default< void, Allocator, void >(allocator)
        {}
    };

    template < typename Key, typename Value, typename Allocator, template <typename,typename,typename> typename Hook >
    class map< Key, Value, Allocator, tag_state, Hook >
        : private dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, Hook >, tag_state >
        , public Hook < map< Key, Value, Allocator, tag_state, Hook >, Allocator, map < Key, Value, Allocator, tag_delta > >
    {
    public:
        using allocator_type = Allocator;
        using tag_type = tag_state;

        using dot_kernel_type = dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, Hook >, tag_state >;
        using iterator = typename dot_kernel_type::iterator;
        using value_type = Value;
        
        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta)
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

        template < typename AllocatorT, typename TagT = tag_state, template <typename, typename, typename> typename HookT = Hook > struct rebind
        {
            using other = map< Key, typename Value::template rebind< AllocatorT, TagT, HookT >::other, AllocatorT, TagT, HookT >;
        };     

        map(allocator_type& allocator)
            : Hook < map< Key, Value, Allocator, tag_state, Hook >, Allocator, map< Key, Value, Allocator, tag_delta > >(allocator)
        {}

        /*
            // TODO: to follow the rule that only delta can merge elsewhere, we need to convert value to delta first.

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            typename Value::rebind< Allocator, tag_delta, crdt::hook_default >::other delta(value);
            return insert(key, delta);
        }
        */

        template < typename ValueT > std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const ValueT& value)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);
            
            typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);

            // TODO: move value
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
                auto pairb = insert(key, typename Value::rebind< Allocator, tag_delta, crdt::hook_default >::other(this->get_allocator()));
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
            return const_cast<decltype(this)>(this)->at(key);
        }

        void clear()
        {
            if (!empty())
            {
                auto allocator = get_allocator();
                arena< 8192 > arena;
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

                typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);
                clear(delta);

                merge(delta);
                commit_delta(std::move(delta));
            }
        }

        iterator erase(iterator it)
        {
            erase_context context;
            erase(it, context);
            return context.iterator;
        }

        size_t erase(const Key& key)
        {
            auto it = find(key);
            if (it != end())
            {
                erase_context context;
                erase(it, context);
                return context.count;
            }

            return 0;
        }

        using dot_kernel_type::size;
        using dot_kernel_type::find;
        using dot_kernel_type::empty;
        using dot_kernel_type::begin;
        using dot_kernel_type::end;

        using dot_kernel_type::merge;
        using dot_kernel_type::get_replica;
        using dot_kernel_type::get_values;

    private:
        friend class dot_kernel_type;

        void update(const Key& key)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

            typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);

            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);

            merge(delta);
            commit_delta(std::move(delta));
        }

        template < typename Delta > void clear(Delta& delta)
        {
            dot_kernel_type::clear(delta);
        }

        void erase(iterator it, typename dot_kernel_type::erase_context& context)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

            typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);

            // TODO: this can move dots
            delta.add_counter_dots(it.it_->second.dots);

            merge(delta, context);
            commit_delta(std::move(delta));
        }
    };
}