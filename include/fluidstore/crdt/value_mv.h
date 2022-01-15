#pragma once

#include <fluidstore/crdt/set.h>
#include <fluidstore/crdt/detail/traits.h>

namespace crdt
{
    namespace detail 
    {
        template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook = hook_default >
        class value_mv;

        template < typename Value, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class value_mv< Value, Allocator, tag_delta, MetadataTag, Hook >
        {
            template < typename ValueT, typename AllocatorT, typename TagT, typename MetadataTagT, template <typename, typename, typename> typename HookT > friend class value_mv;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_delta;

            template < typename AllocatorT, typename TagT = tag_delta, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = value_mv< Value, AllocatorT, TagT, MetadataTagT, HookT >;

            template < typename AllocatorT > value_mv(AllocatorT&& allocator)
                : values_(std::forward< AllocatorT >(allocator))
            {}

            template < typename ValueMv > void merge(const ValueMv& other)
            {
                values_.merge(other.values_);
            }

            allocator_type& get_allocator()
            {
                return values_.get_allocator();
            }

            void reset()
            {
                values_.reset();
            }

        private:
            crdt::set< Value, Allocator, tag_delta, MetadataTag, Hook > values_;
        };

        template < typename Value, typename Allocator, typename MetadataTag, template <typename, typename, typename> typename Hook >
        class value_mv< Value, Allocator, tag_state, MetadataTag, Hook >
            : public Hook < value_mv< Value, Allocator, tag_state, MetadataTag, Hook >, Allocator, value_mv< Value, Allocator, tag_delta, MetadataTag > >
        {
            template < typename ValueT, typename AllocatorT, typename TagT, typename MetadataTagT, template <typename, typename, typename> typename HookT > friend class value_mv;

        public:
            using allocator_type = Allocator;
            using tag_type = tag_state;
            using value_type = Value;

            template < typename AllocatorT, typename TagT = tag_state, typename MetadataTagT = MetadataTag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = value_mv< Value, AllocatorT, TagT, MetadataTagT, HookT >;

            using delta_type = rebind_t< Allocator, tag_delta, MetadataTag, crdt::hook_default >;

            struct delta_extractor
            {
                template < typename Container, typename Delta > void apply(Container& instance, Delta& delta)
                {
                    delta.values_.merge(instance.values_.extract_delta());
                }
            };

            template < typename AllocatorT > value_mv(AllocatorT&& allocator)
                : Hook < value_mv< Value, Allocator, tag_state, MetadataTag, Hook >, Allocator, value_mv< Value, Allocator, tag_delta, MetadataTag > >(std::forward< AllocatorT >(allocator))
                , values_(this->get_allocator())
            {}

            Value get_one() const
            {
                switch (values_.size())
                {
                case 0:
                    return Value();
                case 1:
                    return *values_.begin();
                default:
                    // TODO: this needs some better interface
                    std::abort();
                }
            }

            const auto& get_all() const { return values_; }

            void set(Value value)
            {
                auto allocator = this->get_allocator();

                memory::static_buffer< temporary_buffer_size > buffer;
                memory::buffer_allocator< void, decltype(buffer), std::allocator< void > > buffer_allocator(buffer);
                crdt::allocator< typename decltype(allocator)::replica_type, void, decltype(buffer_allocator) > tmp(allocator.get_replica(), buffer_allocator);

                typename delta_type::template rebind_t< decltype(tmp) > delta(tmp);

                values_.delta_clear(delta.values_);
                values_.delta_insert(delta.values_, value);
                values_.merge(delta.values_);

                this->commit_delta(std::move(delta));
            }

            template < typename ValueMv > void merge(const ValueMv& other)
            {
                values_.merge(other.values_);
            }

            auto& operator = (const Value& value)
            {
                set(value);
                return *this;
            }

            template < typename ValueMv >
            auto& operator = (const ValueMv& other)
            {
                merge(other);
                return *this;
            }

            bool operator == (const Value& value) const
            {
                return values_.size() <= 1 ? get_one() == value : false;
            }

            template < typename ValueMv >
            bool operator == (const ValueMv& other) const
            {
                return values_ == other.values_;
            }

        private:
            crdt::set< Value, Allocator, tag_state, MetadataTag, Hook > values_;
        };

        template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook, bool > struct rebind_value;

        template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook > struct rebind_value< Value, Allocator, Tag, MetadataTag, Hook, true >
        {
            using type = typename Value::template rebind_t< Allocator, Tag, MetadataTag, Hook >;
        };

        template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook > struct rebind_value< Value, Allocator, Tag, MetadataTag, Hook, false >
        {
            using type = Value;
        };

        template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook, bool > using rebind_value_t = typename detail::rebind_value< Value, Allocator, Tag, MetadataTag, Hook, false >::type;
    }  
            
    template < typename Value, typename Allocator = void, typename Tag = void, typename MetadataTag = void, template <typename, typename, typename> typename Hook = hook_default >
    class value_mv
        : public detail::value_mv < typename detail::rebind_value_t< Value, Allocator, Tag, MetadataTag, Hook, is_crdt_type< Value >::value >, Allocator, Tag, MetadataTag, Hook >
    {
    public:
        template < typename AllocatorT > value_mv(AllocatorT&& allocator)
            : detail::value_mv< typename detail::rebind_value_t< Value, Allocator, Tag, MetadataTag, Hook, is_crdt_type< Value >::value >, Allocator, Tag, MetadataTag, Hook >(std::forward< AllocatorT >(allocator))
        {}
    };

    template < typename Value, typename Allocator, typename Tag, typename MetadataTag, template <typename, typename, typename> typename Hook > struct is_crdt_type < value_mv< Value, Allocator, Tag, MetadataTag, Hook > > : std::true_type {};
        
    template < typename Value >
    class value_mv< Value, void, void, void, hook_default >
    {
    public:
        template < typename AllocatorT, typename TagT = void, typename MetadataTagT = void, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = value_mv< Value, AllocatorT, TagT, MetadataTagT, HookT >;
    };


}