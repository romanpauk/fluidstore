#pragma once

#include <fluidstore/crdts/dot_kernel.h>

namespace crdt
{
    template < typename Container > struct hook_none 
    {
        hook_none(typename Container::allocator_type&) {}

        template < typename Delta > void commit_delta(Delta&& delta)
        {}
    };

    template < typename Container > struct hook_extract
    {
        hook_extract(typename Container::allocator_type& allocator)
            : delta_(allocator)
        {}

        template < typename Delta > void commit_delta(Delta&& delta)
        {
            delta_.merge(delta);
        }

        Container extract_delta()
        {
            Container delta(std::move(delta_));
            return delta;
        }

    private:
        Container delta_;        
    };

    template < typename Key, typename Allocator, typename Tag, template <typename> typename Hook = hook_none >
    class set2;

    template < typename Key, typename Allocator, template <typename> typename Hook >
    class set2< Key, Allocator, tag_delta, Hook >
        : public dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_delta, Hook >, tag_delta >
    {
    public:
        using allocator_type = Allocator;

        set2(allocator_type& allocator)
            : allocator_(allocator)
        {}

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    private:
        allocator_type& allocator_;
    };

    template < typename Key, typename Allocator, template <typename> typename Hook >
    class set2< Key, Allocator, tag_state, Hook >
        : public dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_state, Hook >, tag_state >
        , public Hook < set2 < Key, Allocator, tag_delta > >
    {
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_state, Hook >, tag_state >;
        using iterator = typename dot_kernel_type::iterator;

    public:
        using allocator_type = Allocator;

        set2(allocator_type& allocator)
            : Hook< set2< Key, Allocator, tag_delta > >(allocator)
            , allocator_(allocator)
        {}

        std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);

            set2< Key, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            delta_insert(delta, key);

            insert_context context;
            merge(delta, context);
            commit_delta(std::move(delta));
            return { context.result.first, context.result.second };
        }

        template < typename Delta > void delta_insert(Delta& delta, const Key& key)
        {
            auto dot = get_next_dot();
            delta.add_counter_dot(dot);
            delta.add_value(key, dot);
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
                arena_allocator< void > arenaallocator(arena);
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
                
                set2< Key, decltype(deltaallocator), tag_delta > delta(deltaallocator);
                clear(delta);

                merge(delta);
                commit_delta(std::move(delta));
            }
        }

        allocator_type& get_allocator()
        {
            return allocator_;
        }

        template < typename Delta > void clear(Delta& delta)
        {
            dot_kernel_type::clear(delta);
        }

    private:
        void erase(iterator it, typename dot_kernel_type::erase_context & context)
        {
            auto allocator = get_allocator();
            arena< 8192 > arena;
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);

            set2< Key, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            delta.add_counter_dots(it.it_->second.dots);
                        
            merge(delta, context);
            commit_delta(std::move(delta));
        }

        allocator_type& allocator_;
    };
}