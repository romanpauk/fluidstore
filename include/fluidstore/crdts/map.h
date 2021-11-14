#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/set.h>

#include <stdexcept>

namespace crdt
{
    template < typename Key, typename Value, typename Allocator, typename Tag, template <typename,typename,typename> typename Hook = hook_default >
    class map;

    template < typename Key, typename Value, typename Allocator >
    class map< Key, Value, Allocator, tag_delta, hook_default >
        : public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_delta, hook_default >, tag_delta >
    {
    public:
        using allocator_type = Allocator;

        template < typename AllocatorT > struct rebind
        {
            using other = map< Key, typename Value::template rebind< AllocatorT >::other, AllocatorT, tag_delta, hook_default >;
        };

        map(allocator_type& allocator)
            : allocator_(allocator)
        {}

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    private:
        allocator_type allocator_;
    };

    template < typename Key, typename Value, typename Allocator, template <typename,typename,typename> typename Hook >
    class map< Key, Value, Allocator, tag_state, Hook >
        : public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, Hook >, tag_state >
        , public Hook < map< Key, Value, Allocator, tag_state, Hook >, Allocator, map < Key, Value, Allocator, tag_delta > >
    {
    public:
        using allocator_type = Allocator;
        using dot_kernel_type = dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, Hook >, tag_state >;
        using iterator = typename dot_kernel_type::iterator;

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

        template < typename AllocatorT > struct rebind
        {
            using other = map< Key, typename Value::template rebind< AllocatorT >::other, AllocatorT, tag_state, Hook >;
        };

        map(allocator_type& allocator)
            : Hook < map< Key, Value, Allocator, tag_state, Hook >, Allocator, map< Key, Value, Allocator, tag_delta > >(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const Value& value)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            
            map< Key, Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
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
            return const_cast<decltype(this)>(this)->at(key);
        }

        void clear()
        {
            if (!empty())
            {
                auto allocator = get_allocator();
                arena< 8192 > arena;
                arena_allocator< void > arenaallocator(arena);
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);

                map< Key, Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
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

        template < typename Delta > void clear(Delta& delta)
        {
            dot_kernel_type::clear(delta);
        }

        void update(const Key& key)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            
            map< Key, Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);

            merge(delta);
            commit_delta(std::move(delta));
        }

    private:
        void erase(iterator it, typename dot_kernel_type::erase_context& context)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);

            map< Key, Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            delta.add_counter_dots(it.it_->second.dots);

            merge(delta, context);
            commit_delta(std::move(delta));
        }
    };
}