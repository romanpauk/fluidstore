#pragma once

#include <fluidstore/crdt/detail/dot_kernel.h>
#include <fluidstore/crdt/set.h>

#include <stdexcept>

namespace crdt
{
    namespace detail
    {
        template < typename Key, typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook = hook_default >
        class map;

        template < typename Key, typename Value, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class map< Key, Value, Allocator, tag_delta, MetadataTag, Hook >
            : public hook_default< void, Allocator, void >
            , public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_delta, MetadataTag, Hook >, tag_delta, MetadataTag >
        {
            using dot_kernel_type = dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_delta, MetadataTag, Hook >, tag_delta >;
            using hook_type = hook_default< void, Allocator, void >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            template < typename AllocatorT, typename TagT = tag_delta, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = map< Key, typename Value::template rebind_t< AllocatorT, TagT, MetadataTagT, HookT >, AllocatorT, TagT, MetadataTagT, HookT >;

            template < typename AllocatorT > map(AllocatorT&& allocator)
                : hook_default< void, Allocator, void >(std::forward< AllocatorT >(allocator))
            {}

            map(map< Key, Value, Allocator, tag_delta, MetadataTag, Hook >&&) = default;

            map< Key, Value, Allocator, tag_delta, MetadataTag, Hook >& operator = (map< Key, Value, Allocator, tag_delta, MetadataTag, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            }
        };

        template < typename Key, typename Value, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class map< Key, Value, Allocator, tag_state, MetadataTag, Hook >
            : public Hook < map< Key, Value, Allocator, tag_state, MetadataTag, Hook >, Allocator, map < Key, Value, Allocator, tag_delta, MetadataTag > >
            , public dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, MetadataTag, Hook >, tag_state, MetadataTag >
        {
            using hook_type = Hook < map< Key, Value, Allocator, tag_state, MetadataTag, Hook >, Allocator, map < Key, Value, Allocator, tag_delta, MetadataTag > >;
            using dot_kernel_type = dot_kernel< Key, Value, Allocator, map< Key, Value, Allocator, tag_state, MetadataTag, Hook >, tag_state, MetadataTag >;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_state;            
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
                            typename std::remove_reference_t< decltype(lvalue) >::delta_extractor extractor;
                            rvalue.merge(lvalue.extract_delta());
                            extractor.apply(lvalue, rvalue);
                        }
                    }
                }
            };

            template < typename AllocatorT, typename TagT = tag_state, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = map< Key, typename Value::template rebind_t< AllocatorT, TagT, MetadataTagT, HookT >, AllocatorT, TagT, MetadataTagT, HookT >;

            using delta_type = rebind_t< Allocator, tag_delta, MetadataTag, crdt::hook_default >;

            template < typename AllocatorT > map(AllocatorT&& allocator)
                : hook_type(std::forward< AllocatorT >(allocator))
            {}

            map(map< Key, Value, Allocator, tag_state, MetadataTag, Hook >&&) = default;

            map< Key, Value, Allocator, tag_state, MetadataTag,Hook >& operator = (map< Key, Value, Allocator, tag_state, MetadataTag, Hook > && other)
            {
                std::swap(static_cast<hook_type&>(*this), static_cast<hook_type&>(other));
                std::swap(static_cast<dot_kernel_type&>(*this), static_cast<dot_kernel_type&>(other));
                return *this;
            }
           
            template < typename ValueT > std::pair< typename dot_kernel_type::iterator, bool > insert(const Key& key, const ValueT& value)
            {
                auto allocator = this->get_allocator();
                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);

                typename delta_type::template rebind_t< decltype(tmp) > delta(tmp);

                // TODO: move value
                auto dot = this->get_next_dot();
                delta.add_counter_dot(dot);
                delta.add_value(key, dot, value);

                typename dot_kernel_type::insert_context context;
                merge(delta, context);
                this->commit_delta(delta);
                return { context.result.first, context.result.second };
            }

            auto& operator[](const Key& key)
            {
                if constexpr (std::uses_allocator_v< Value, Allocator >)
                {
                    // TODO: this Value should not be tracked, it is temporary.
                    auto pairb = insert(key, typename Value::delta_type(this->get_allocator()));
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
                    auto allocator = this->get_allocator();

                    memory::static_buffer< temporary_buffer_size > buffer;
                    memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                    crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);

                    typename delta_type::template rebind_t< decltype(tmp) > delta(tmp);
                    clear(delta);

                    merge(delta);
                    this->commit_delta(std::move(delta));
                }
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

            using dot_kernel_type::size;
            using dot_kernel_type::find;
            using dot_kernel_type::empty;
            using dot_kernel_type::begin;
            using dot_kernel_type::end;

            using dot_kernel_type::merge;
            using dot_kernel_type::get_replica_map;
            using dot_kernel_type::get_values;

        private:
            friend dot_kernel_type;

            void update(const Key& key)
            {
                auto allocator = this->get_allocator();
                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);
                
                typename delta_type::template rebind_t< decltype(tmp) > delta(tmp);

                auto dot = this->get_next_dot();
                delta.add_counter_dot(dot);
                delta.add_value(key, dot);

                merge(delta);
                this->commit_delta(std::move(delta));
            }

            template < typename Delta > void clear(Delta& delta)
            {
                dot_kernel_type::clear(delta);
            }

            void erase(iterator it, typename dot_kernel_type::erase_context& context)
            {
                auto allocator = this->get_allocator();
                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);

                typename delta_type::template rebind_t< decltype(tmp) > delta(tmp);

                // TODO: this can move dots
                delta.add_counter_dots(it.it_->second.dots);

                merge(delta, context);
                this->commit_delta(std::move(delta));
            }
        };
    }

    template < typename Key, typename Value, typename Allocator = void, typename Tag = void, typename MetadataTag = void, template <typename, typename, typename> typename Hook = hook_default >
    class map
        : public detail::map < Key, typename Value::template rebind_t< Allocator, Tag, MetadataTag, Hook >, Allocator, Tag, MetadataTag, Hook >
    {
        using base_type = detail::map < Key, typename Value::template rebind_t< Allocator, Tag, MetadataTag, Hook >, Allocator, Tag, MetadataTag, Hook >;

    public:
        template < typename AllocatorT > map(AllocatorT&& allocator)
            : detail::map< Key, typename Value::template rebind_t< Allocator, tag_state, MetadataTag, Hook >, Allocator, Tag, MetadataTag, Hook >(std::forward< AllocatorT >(allocator))
        {}

        map(map< Key, Value, Allocator, Tag, MetadataTag, Hook >&&) = default;

        map< Key, Value, Allocator, Tag, MetadataTag, Hook >& operator = (map< Key, Value, Allocator, Tag, MetadataTag, Hook > && other)
        {
            std::swap(static_cast<base_type&>(*this), static_cast<base_type&>(other));
            return *this;
        }
    };

    template < typename Key, typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook > struct is_crdt_type < map< Key, Value, Allocator, Tag, MetadataTag, Hook > > : std::true_type {};

    template < typename Key, typename Value >
    class map< Key, Value, void, void, void, hook_default >
    {
    public:
        template < typename AllocatorT, typename TagT = void, typename MetadataTagT = void, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = map< Key, Value, AllocatorT, TagT, MetadataTagT, HookT >;
    };
}