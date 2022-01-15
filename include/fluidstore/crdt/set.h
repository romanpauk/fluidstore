#pragma once

#include <fluidstore/crdt/detail/dot_kernel.h>
#include <fluidstore/crdt/detail/traits.h>

#include <fluidstore/crdt/hooks/hook_default.h>

namespace crdt
{
    namespace detail
    {
        template < typename Key, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook = hook_default >
        class set;

        template < typename Key, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class set< Key, Allocator, tag_delta, MetadataTag, Hook >
            : public hook_default< void, Allocator, void >
            , public dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, MetadataTag, Hook >, tag_delta, MetadataTag >
        {
            using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_delta, MetadataTag, Hook >, tag_delta, MetadataTag >;
            using hook_type = hook_default< void, Allocator, void >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            using iterator = typename dot_kernel_type::iterator;

            template < typename AllocatorT, typename TagT = tag_delta, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = set< Key, AllocatorT, TagT, MetadataTagT, HookT >;

            struct delta_extractor
            {
                template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
            };

            template < typename AllocatorT > set(AllocatorT&& allocator)
                : hook_default< void, Allocator, void >(std::forward< AllocatorT >(allocator))
            {}

            set(set< Key, Allocator, tag_delta, MetadataTag, Hook >&& other) = default;

            set< Key, Allocator, tag_delta, MetadataTag, Hook >& operator = (set< Key, Allocator, tag_delta, MetadataTag, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            };
        };

        template < typename Key, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class set< Key, Allocator, tag_state, MetadataTag, Hook >
            : public Hook < set< Key, Allocator, tag_state, MetadataTag, Hook >, Allocator, set< Key, Allocator, tag_delta, MetadataTag > >
            , public dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, MetadataTag, Hook >, tag_state, MetadataTag >
        {
            using dot_kernel_type = dot_kernel< Key, void, Allocator, set< Key, Allocator, tag_state, MetadataTag, Hook >, tag_state, MetadataTag >;
            using hook_type = Hook < set< Key, Allocator, tag_state, MetadataTag, Hook >, Allocator, set< Key, Allocator, tag_delta, MetadataTag > >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            using iterator = typename dot_kernel_type::iterator;

            template < typename AllocatorT, typename TagT = tag_state, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = set< Key, AllocatorT, TagT, MetadataTagT, HookT >;

            using delta_type = rebind_t< allocator_type, tag_delta, MetadataTag, crdt::hook_default >;

            struct delta_extractor
            {
                template < typename Container, typename Delta > void apply(Container& instance, Delta& delta) {}
            };

            template < typename AllocatorT > set(AllocatorT&& allocator)
                : hook_type(std::forward< AllocatorT >(allocator))
            {}

            set(set< Key, Allocator, tag_state, MetadataTag, Hook >&&) = default;
            
            set< Key, Allocator, tag_state, MetadataTag, Hook >& operator = (set< Key, Allocator, tag_state, MetadataTag, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            }

            std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key)
            {
                auto allocator = this->get_allocator();
                
                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > deltaallocator(allocator.get_replica(), buffer_allocator);

                typename delta_type::template rebind_t< decltype(deltaallocator) > delta(deltaallocator);
                delta_insert(delta, key);

                typename dot_kernel_type::insert_context context;
                merge(delta, context);
                this->commit_delta(std::move(delta));
                return { context.result.first, context.result.second };
            }

            iterator erase(iterator it)
            {
                typename dot_kernel_type::erase_context context;
                erase(it, context);
                return context.iterator;
            }

            size_t erase(const Key& key)
            {
                auto it = find(key);
                if (it != end())
                {
                    typename dot_kernel_type::erase_context context;
                    erase(it, context);
                    return context.count;
                }

                return 0;
            }

            void clear()
            {
                if (!empty())
                {
                    auto allocator = this->get_allocator();

                    memory::static_buffer< temporary_buffer_size > buffer;
                    memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                    crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > deltaallocator(allocator.get_replica(), buffer_allocator);

                    typename delta_type::template rebind_t< decltype(deltaallocator) > delta(deltaallocator);
                    delta_clear(delta);

                    merge(delta);
                    this->commit_delta(std::move(delta));
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
            template < typename ValueT, typename AllocatorT, typename TagT, typename MetadataTagT, template <typename, typename, typename> typename HookT > friend class value_mv;

            template < typename Delta > void delta_clear(Delta& delta)
            {
                dot_kernel_type::clear(delta);
            }

            template < typename Delta > void delta_insert(Delta& delta, const Key& key)
            {
                auto dot = this->get_next_dot();
                delta.add_counter_dot(dot);
                delta.add_value(key, dot);
            }

            void erase(iterator it, typename dot_kernel_type::erase_context& context)
            {
                auto allocator = this->get_allocator();

                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > deltaallocator(allocator.get_replica(), buffer_allocator);

                typename delta_type::template rebind_t< decltype(deltaallocator) > delta(deltaallocator);
                delta.add_counter_dots(it.it_->second.dots);

                merge(delta, context);
                this->commit_delta(std::move(delta));
            }
        };
    }

    template < typename Key, typename Allocator = void, typename Tag = void, typename MetadataTag = void, template <typename, typename, typename> typename Hook = hook_default >
    class set
        : public detail::set< Key, Allocator, Tag, MetadataTag, Hook >
    {
    public:
        template < typename AllocatorT > set(AllocatorT&& allocator)
            : detail::set< Key, Allocator, Tag, MetadataTag, Hook >(std::forward< AllocatorT >(allocator))
        {}
    };

    template < typename Key, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook > struct is_crdt_type < set< Key, Allocator, Tag, MetadataTag, Hook > > : std::true_type {};

    template < typename Key >
    class set< Key, void, void, void, hook_default >
    {
    public:
        template < typename AllocatorT, typename TagT = void, typename MetadataTagT = void, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = set< Key, AllocatorT, TagT, MetadataTagT, HookT >;
    };
}