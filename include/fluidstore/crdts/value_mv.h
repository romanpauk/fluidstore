#pragma once

#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/allocator_traits.h>

namespace crdt
{
    template < typename Value, typename Allocator, typename Tag, typename Hook, typename Delta > class value_mv_base
        : public Hook::template hook< Allocator, Delta, value_mv_base< Value, Allocator, Tag, Hook, Delta > >
    {
        typedef value_mv_base< Value, Allocator, Tag, Hook, Delta > value_mv_base_type;
        template < typename Value, typename Allocator, typename Tag, typename Hook, typename Delta > friend class value_mv_base;
        typedef typename Hook::template hook< Allocator, Delta, value_mv_base_type > hook_type;

    public:
        typedef Allocator allocator_type;
        
        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = value_mv_base< Value, AllocatorT, Tag, HookT, Delta >;
        };

        struct delta_extractor
        {
            template < typename Delta > void apply(value_mv_base_type& instance, Delta& delta)
            {
                delta.values_.merge(instance.values_.extract_delta());
            }
        };

        value_mv_base(allocator_type allocator)
            : hook_type(allocator, allocator.get_replica().generate_instance_id())
            , values_(allocator)
        {}

        value_mv_base(allocator_type allocator, typename allocator_type::replica_type::id_type id)
            : hook_type(allocator, id)
            , values_(allocator)
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
            arena_allocator< void > arenaallocator(arena);
            crdt::allocator< typename decltype(allocator)::replica_type, void, arena_allocator< void > > deltaallocator(allocator.get_replica(), arenaallocator);
            auto delta = mutable_delta(deltaallocator);

            values_.clear(delta.values_);
            values_.insert(delta.values_, value);
            values_.merge(delta.values_);
            this->commit_delta(delta);
        }

        template < typename ValueMv > void merge(const ValueMv& other)
        {
            values_.merge(other.values_);
        }

        value_mv_base_type& operator = (const Value& value)
        {
            set(value);
            return *this;
        }

        template < typename ValueT, typename AllocatorT, typename TagT, typename HookT, typename DeltaT > 
        value_mv_base_type& operator = (const value_mv_base< ValueT, AllocatorT, TagT, HookT, DeltaT >& value)
        {
            merge(other);
            return *this;
        }

        bool operator == (const Value& value) const
        {
            return values_.size() <= 1 ? get_one() == value : false;
        }

        template < typename ValueT, typename AllocatorT, typename TagT, typename HookT, typename DeltaT >
        bool operator == (const value_mv_base< ValueT, AllocatorT, TagT, HookT, DeltaT >& other) const
        {
            return values_ == other.values_;
        }

        crdt::set< Value, Allocator, Hook > values_;
    };

    template < typename Value, typename Allocator, typename Hook > class value_mv_base< Value, Allocator, tag_delta, Hook, void >
    {
        typedef value_mv_base< Value, Allocator, tag_delta, Hook, void > value_mv_base_type;

    public:
        typedef Allocator allocator_type;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            using other = value_mv_base< Value, AllocatorT, tag_delta, HookT, void >;
        };

        value_mv_base(allocator_type allocator)
            : values_(allocator)
        {}

        template < typename ValueMv > void merge(const ValueMv& other)
        {
            values_.merge(other.values_);
        }

        const typename allocator_type::replica_type::id_type& get_id() const { return values_.get_id(); }
        auto get_allocator() { return values_.get_allocator(); }

        crdt::set_base< Value, Allocator, tag_delta, Hook, void > values_;
    };

    template < typename Value, typename Allocator, typename Hook = default_state_hook, 
        typename Delta = value_mv_base< 
            Value, // TODO: Value can be recursive
            typename allocator_traits< Allocator >::template allocator_type< tag_delta >,
            tag_delta, default_delta_hook, void
        >
    > class value_mv
        : public value_mv_base< Value, Allocator, tag_state, Hook, Delta >
    {
        typedef value_mv< Value, Allocator, Hook, Delta > value_mv_type;
        typedef value_mv_base< Value, Allocator, tag_state, Hook, Delta > value_mv_base_type;

    public:
        typedef Allocator allocator_type;
        typedef Hook hook_type;
        typedef Delta delta_type;

        template < typename AllocatorT, typename HookT = Hook > struct rebind
        {
            typedef value_mv< Value, AllocatorT, HookT, Delta > other;
        };

        value_mv(allocator_type allocator)
            : value_mv_base_type(allocator, allocator.get_replica().generate_instance_id())
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id)
            : value_mv_base_type(allocator, id)
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id, const Value& value)
            : value_mv_base_type(allocator, id)
        {
            set(value);
        }
    };
}