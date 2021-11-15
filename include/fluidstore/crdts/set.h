#pragma once

#include <fluidstore/crdts/dot_kernel.h>
#include <fluidstore/crdts/hook_default.h>

namespace crdt
{
    template < typename Key, typename Allocator, typename Tag = crdt::tag_state , template <typename, typename, typename> typename Hook = hook_default >
    class set;

    template < typename Key, typename Allocator, template <typename, typename, typename> typename Hook >
    class set< Key, Allocator, tag_delta, Hook >
        : public dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, Hook >, tag_delta >
        , public hook_default< void, Allocator, void >
    {
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, Hook >, tag_delta >;

    public:
        using allocator_type = Allocator;
        using tag_type = tag_delta;

        using iterator = typename dot_kernel_type::iterator;

        template < typename AllocatorT, typename TagT = tag_delta, template <typename, typename, typename> typename HookT = Hook > struct rebind
        {
            using other = set< Key, AllocatorT, TagT, HookT >;
        };

        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
        };

        set(allocator_type& allocator)
            : hook_default< void, Allocator, void >(allocator)
        {}        
    };

    template < typename Key, typename Allocator, template <typename,typename,typename> typename Hook >
    class set< Key, Allocator, tag_state, Hook >
        : private dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, Hook >, tag_state >
        , public Hook < set< Key, Allocator, tag_state, Hook >, Allocator, set< Key, Allocator, tag_delta > >
    {
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, Hook >, tag_state >;
        
    public:
        using allocator_type = Allocator;
        using tag_type = tag_delta;

        using iterator = typename dot_kernel_type::iterator;

        template < typename AllocatorT, typename TagT = tag_state, template <typename,typename,typename> typename HookT = Hook > struct rebind
        {
            using other = set< Key, AllocatorT, TagT, HookT >;
        };

        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
        };

        set(allocator_type& allocator)
            : Hook< set< Key, Allocator, tag_state, Hook >, Allocator, set< Key, Allocator, tag_delta > >(allocator)
        {}
            
        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);
                        
            typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);
            delta_insert(delta, key);

            insert_context context;
            merge(delta, context);
            commit_delta(std::move(delta));
            return { context.result.first, context.result.second };
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

        void clear()
        {
            if (!empty())
            {
                auto allocator = get_allocator();
                arena< 8192 > arena;
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);
                
                typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);
                delta_clear(delta);

                merge(delta);
                commit_delta(std::move(delta));
            }
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
        template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook >
        friend class value_mv;

        template < typename Delta > void delta_clear(Delta& delta)
        {
            dot_kernel_type::clear(delta);
        }

        template < typename Delta > void delta_insert(Delta& delta, const Key& key)
        {
            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);
        }

        void erase(iterator it, typename dot_kernel_type::erase_context & context)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

            typename rebind< decltype(deltaallocator), tag_delta, crdt::hook_default >::other delta(deltaallocator);
            delta.add_counter_dots(it.it_->second.dots);
                        
            merge(delta, context);
            commit_delta(std::move(delta));
        }
    };
}