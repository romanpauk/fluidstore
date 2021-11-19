#pragma once

#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/traits.h>

namespace crdt
{
    template < typename Value, typename Allocator, typename Tag = crdt::tag_state, template <typename, typename, typename> typename Hook = hook_default >
    class value_mv;

    template < typename Value, typename Allocator = void, typename Tag = void, template <typename, typename, typename> typename Hook = hook_default >
    class value_mv2
    {
    public:
        using allocator_type = Allocator;
        using tag_type = Tag;
        using hook_type = Hook< void, void, void >;

        template < typename AllocatorT, typename TagT = Tag, template <typename, typename, typename> typename HookT = Hook > using rebind_t = value_mv2< Value, AllocatorT, TagT, HookT >;
    };

    template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > struct is_crdt_type < value_mv2< Value, Allocator, Tag, Hook > >: std::true_type {};

    template < typename Value, typename Allocator, template <typename, typename, typename> typename Hook >
    class value_mv< Value, Allocator, tag_delta, Hook >
    {
        template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > friend class value_mv;

    public:
        using allocator_type = Allocator;
        using tag_type = tag_delta;

        template < typename AllocatorT, typename TagT = tag_delta, template <typename, typename, typename> typename HookT = hook_default > using rebind_t = value_mv< Value, AllocatorT, TagT, HookT >;

        value_mv(allocator_type& allocator)
            : values_(allocator)
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
        crdt::set< Value, Allocator, tag_delta > values_;
    };

    template < typename Value, typename Allocator, template <typename, typename, typename> typename Hook >
    class value_mv< Value, Allocator, tag_state, Hook >
        : public Hook < value_mv< Value, Allocator, tag_state, Hook >, Allocator, value_mv< Value, Allocator, tag_delta > >
    {
        template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > friend class value_mv;

    public:
        using allocator_type = Allocator;
        using tag_type = tag_state;
        using value_type = Value;

        template < typename AllocatorT, typename TagT = tag_state, template <typename, typename, typename> typename HookT = Hook > using rebind_t = value_mv< Value, AllocatorT, TagT, HookT >;

        using delta_type = typename rebind_t< Allocator, tag_delta, crdt::hook_default >;

        struct delta_extractor
        {
            template < typename Container, typename Delta > void apply(Container& instance, Delta& delta)
            {
                delta.values_.merge(instance.values_.extract_delta());
            }
        };

        value_mv(allocator_type& allocator)
            : Hook < value_mv< Value, Allocator, tag_state, Hook >, Allocator, value_mv< Value, Allocator, tag_delta > >(allocator)
            , values_(get_allocator())
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
            auto allocator = get_allocator();
            arena< 8192 > arena;
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arena);

            typename delta_type::rebind_t< decltype(deltaallocator) > delta(deltaallocator);

            values_.delta_clear(delta.values_);
            values_.delta_insert(delta.values_, value);
            values_.merge(delta.values_);

            commit_delta(std::move(delta));
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
        crdt::set< Value, Allocator, tag_state, Hook > values_;
    };

    template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook, bool > struct rebind_value;
    
    template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > struct rebind_value< Value, Allocator, Tag, Hook, true >
    {
        using type = typename Value::template rebind_t< Allocator, Tag, Hook >;
    };

    template < typename Value, typename Allocator, typename Tag, template <typename, typename, typename> typename Hook > struct rebind_value< Value, Allocator, Tag, Hook, false >
    {
        using type = Value;
    };

    template < typename Value, typename Allocator, template <typename, typename, typename> typename Hook >
    class value_mv2< Value, Allocator, tag_state, Hook >
        : public value_mv < typename rebind_value< Value, Allocator, tag_state, Hook, is_crdt_type< Value >::value >::type, Allocator, tag_state, Hook >
    {
    public:
        value_mv2(Allocator& allocator)
            : value_mv< typename rebind_value< Value, Allocator, tag_state, Hook, is_crdt_type< Value >::value >::type, Allocator, tag_state, Hook >(allocator)
        {}
    };

}