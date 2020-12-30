#pragma once

#include <fluidstore/crdts/set.h>

#include <memory>

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
            values_.clear(delta_.values_);
            values_.insert(delta_.values_, value);
            values_.merge(delta_.values_);
            this->merge_hook(*this, delta_);
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

        // crdt::set_base< Value, Allocator, Tag, default_hook, void > values_;
        crdt::set< Value, Allocator, default_hook > values_;
    };

    template < typename Value, typename Allocator, typename Hook > class value_mv_base< Value, Allocator, tag_delta, Hook, void >
    {
    public:
        typedef Allocator allocator_type;

        value_mv_base(allocator_type allocator)
            : values_(allocator)
        {}

        template < typename ValueMv > void merge(const ValueMv& other)
        {
            values_.merge(other.values_);
        }

        void reset()
        {
            values_.reset();
        }

        crdt::set_base< Value, Allocator, tag_delta, default_hook, void > values_;
    };

    template < typename Value, typename Allocator, typename Hook = default_hook, typename Delta = value_mv_base< Value, Allocator, tag_delta, Hook, void > > class value_mv
        : public value_mv_base< Value, Allocator, tag_state, Hook, Delta >
    {
        typedef value_mv< Value, Allocator, Hook, Delta > value_mv_type;
        typedef value_mv_base< Value, Allocator, tag_state, Hook, Delta > value_mv_base_type;

        // template < typename Value, typename Allocator, typename Hook, typename Delta > friend class value_mv;
        
    public:
        typedef Allocator allocator_type;
 
        template < typename AllocatorT, typename HookT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value_mv< Value, AllocatorT, HookT, Delta > type;
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