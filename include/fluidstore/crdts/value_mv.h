#pragma once

#include <fluidstore/crdts/set.h>

#include <memory>

namespace crdt
{
    template < typename Value, typename Allocator, typename Tag = tag_state, typename Hook = default_hook< Allocator > > class value_mv
        // : public Allocator::template hook< value_mv< Value, Allocator, Tag > >
    {
        typedef value_mv< Value, Allocator, Tag, Hook > value_mv_type;
        template < typename Value, typename Allocator, typename Tag, typename Hook > friend class value_mv;
        
    public:
        typedef Allocator allocator_type;
        typedef typename allocator_type::template hook< value_mv_type > hook_type;
        typedef typename allocator_type::replica_type replica_type;

        template < typename AllocatorT, typename TagT, typename HookT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value_mv< Value, AllocatorT, TagT, HookT > type;
        };

        value_mv(allocator_type allocator)
            // : hook_type(allocator.get_replica().generate_instance_id())
            : values_(allocator)
            , allocator_(allocator)
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id)
            // : hook_type(id)
            : values_(allocator, id)
            , allocator_(allocator)
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id, const Value& value)
            // : hook_type(id)
            : values_(allocator)
            , allocator_(allocator)
        {
            set(value);
        }

        const typename Allocator::replica_type::id_type& get_id() const { return values_.get_id(); }

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
            arena< 1024 > buffer;
            arena_allocator< void, allocator< typename Allocator::replica_type::delta_replica_type > > allocator(buffer, values_.get_allocator().get_replica());
            typename decltype(values_)::template rebind< decltype(allocator), tag_delta, default_hook< decltype(allocator) > >::type delta(allocator, values_.get_id());

            values_.clear(delta);
            values_.insert(delta, value);

            values_.merge(delta);
            values_.get_allocator().get_replica().merge(values_, delta);
        }

        template < typename ValueMv > void merge(const ValueMv& other)
        {
            values_.merge(other.values_);
        }

        value_mv_type& operator = (const Value& value)
        {
            set(value);
            return *this;
        }

        template < typename ValueT, typename AllocatorT, typename TagT > value_mv_type& operator = (const value_mv< ValueT, AllocatorT, TagT >& value)
        {
            merge(other);
            return *this;
        }

        bool operator == (const Value& value) const 
        { 
            return values_.size() <= 1 ? get_one() == value : false;
        }

        bool operator == (const value_mv< Value, Allocator, Tag >& other) const 
        { 
            return values_ == other.values_;
        }

        allocator_type& get_allocator() { return allocator_; }

    private:
        allocator_type allocator_;
        crdt::set< Value, Allocator, Tag, Hook > values_;
    };
}