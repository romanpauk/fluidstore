#pragma once

#include <fluidstore/crdt/detail/dot_kernel.h>
#include <fluidstore/crdt/detail/traits.h>

#include <fluidstore/crdt/hooks/hook_default.h>

namespace crdt
{
    namespace detail
    {
        template < typename Key, typename Allocator, typename Tag = crdt::tag_state, template <typename, typename, typename> typename Hook = hook_default >
        class set;

        template < typename Key, typename Allocator, template <typename, typename, typename> typename Hook >
        class set< Key, Allocator, tag_delta, Hook >
            : public dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, Hook >, tag_delta >
            , public hook_default< void, Allocator, void >
        {
            using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, Hook >, tag_delta >;
            using hook_type = hook_default< void, Allocator, void >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            using iterator = typename dot_kernel_type::iterator;

            template < typename AllocatorT, typename TagT = tag_delta, template <typename, typename, typename> typename HookT = Hook > using rebind_t = set< Key, AllocatorT, TagT, HookT >;

            struct delta_extractor
            {
                template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
            };

            set(allocator_type& allocator)
                : hook_default< void, Allocator, void >(allocator)
            {}

            set(set< Key, Allocator, tag_delta, Hook >&& other) = default;

            set< Key, Allocator, tag_delta, Hook >& operator = (set< Key, Allocator, tag_delta, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            };
        };

        template < typename Key, typename Allocator, template <typename, typename, typename> typename Hook >
        class set< Key, Allocator, tag_state, Hook >
            : private dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, Hook >, tag_state >
            , public Hook < set< Key, Allocator, tag_state, Hook >, Allocator, set< Key, Allocator, tag_delta > >
        {
            using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, Hook >, tag_state >;
            using hook_type = Hook < set< Key, Allocator, tag_state, Hook >, Allocator, set< Key, Allocator, tag_delta > >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            using iterator = typename dot_kernel_type::iterator;

            template < typename AllocatorT, typename TagT = tag_state, template <typename, typename, typename> typename HookT = Hook > using rebind_t = set< Key, AllocatorT, TagT, HookT >;

            using delta_type = rebind_t< allocator_type, tag_delta, crdt::hook_default >;

            struct delta_extractor
            {
                template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
            };

            set(allocator_type& allocator)
                : hook_type(allocator)
            {}

            set(set< Key, Allocator, tag_state, Hook >&&) = default;
            
            set< Key, Allocator, tag_state, Hook >& operator = (set< Key, Allocator, tag_state, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            }

            std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
            {
                auto allocator = get_allocator();
                arena< 8192 > arena;
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

                typename delta_type::rebind_t< decltype(deltaallocator) > delta(deltaallocator);
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

                    typename delta_type::rebind_t< decltype(deltaallocator) > delta(deltaallocator);
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
            using dot_kernel_type::get_replica_map;
            using dot_kernel_type::get_values;

        private:
            template < typename ValueT, typename AllocatorT, typename TagT, template <typename, typename, typename> typename HookT > friend class value_mv;

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

            void erase(iterator it, typename dot_kernel_type::erase_context& context)
            {
                auto allocator = get_allocator();
                arena< 8192 > arena;
                crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

                typename delta_type::rebind_t< decltype(deltaallocator) > delta(deltaallocator);
                delta.add_counter_dots(it.it_->second.dots);

                merge(delta, context);
                commit_delta(std::move(delta));
            }
        };
    }

    template < typename Key, typename Allocator = void, typename Tag = void, template <typename, typename, typename> typename Hook = hook_default >
    class set
        : public detail::set< Key, Allocator, Tag, Hook >
    {
    public:
        set(Allocator& allocator)
            : detail::set< Key, Allocator, Tag, Hook >(allocator)
        {}
    };

    template < typename Key, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > struct is_crdt_type < set< Key, Allocator, Tag, Hook > > : std::true_type {};

    template < typename Key >
    class set< Key, void, void, hook_default >
    {
    public:
        template < typename AllocatorT, typename TagT = void, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = set< Key, AllocatorT, TagT, HookT >;
    };
}