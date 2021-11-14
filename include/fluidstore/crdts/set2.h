#pragma once

#include <fluidstore/crdts/dot_kernel.h>

namespace crdt
{
    template < typename Container, typename Allocator, typename Delta > struct hook_none 
    {
        hook_none() {}
        hook_none(Allocator&)
        {}

        template < typename DeltaT > void commit_delta(DeltaT&& delta)
        {}
    };

    template < typename Container, typename Allocator, typename Delta > struct hook_extract
    {
        hook_extract(Allocator& allocator)
            : delta_(allocator)
        {}

        template < typename Delta > void commit_delta(Delta&& delta)
        {
            delta_.merge(delta);
            // delta_.get_allocator().update();
        }

        Delta extract_delta()
        {
            Delta delta(delta_.get_allocator());
            delta.merge(delta_);

            typename Container::delta_extractor extractor;
            extractor.apply(*static_cast<Container*>(this), delta);

            delta_.reset();
            return delta;
        }

        typename Allocator& get_allocator()
        {
            return delta_.get_allocator();
        }

    private:
        Delta delta_;
    };

    template < typename Key, typename Allocator, typename Tag, template <typename,typename,typename> typename Hook = hook_none >
    class set2;

    template < typename Key, typename Allocator >
    class set2< Key, Allocator, tag_delta, hook_none >
        : public dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_delta, hook_none >, tag_delta >
        //, public hook_none< set2< Key, Allocator, tag_delta, hook_none > >
    {
    public:
        using allocator_type = Allocator;

        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
        };

        set2(allocator_type& allocator)
            : allocator_(allocator)
        {}    

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    private:
        allocator_type allocator_;
    };

    template < typename Key, typename Allocator, template <typename,typename,typename> typename Hook >
    class set2< Key, Allocator, tag_state, Hook >
        : public dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_state, Hook >, tag_state >
        , public Hook < set2< Key, Allocator, tag_state, Hook >, Allocator, set2 < Key, Allocator, tag_delta > >
    {
        using dot_kernel_type = dot_kernel< Key, void, Allocator, set2< Key, Allocator, tag_state, Hook >, tag_state >;
        using iterator = typename dot_kernel_type::iterator;

    public:
        using allocator_type = Allocator;

        template < typename AllocatorT > struct rebind
        {
            using other = set2< Key, AllocatorT, tag_state, Hook >;
        };

        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
        };

        set2(allocator_type& allocator)
            : Hook< set2< Key, Allocator, tag_state, Hook >, Allocator, set2< Key, Allocator, tag_delta > >(allocator_)
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

        allocator_type allocator_;
    };
}