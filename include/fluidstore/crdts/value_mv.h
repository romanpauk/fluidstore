#pragma once

#include <fluidstore/crdts/set.h>

#include <memory>

namespace crdt
{
    template < typename Value, typename Allocator > class value_mv
    {
        typedef value_mv< Value, Allocator > value_mv_type;
        template < typename Value, typename Allocator > friend class value_mv;

    public:
        typedef Allocator allocator_type;

        template < typename AllocatorT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value_mv< Value, AllocatorT > type;
        };

        value_mv(allocator_type allocator)
            : values_(allocator)
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id)
            : values_(allocator, id)
        {}

        value_mv(allocator_type allocator, typename Allocator::replica_type::id_type id, const Value& value)
            : values_(allocator, id)
        {
            set(value);
        }

        const typename Allocator::replica_type::id_type& get_id() const { return values_.get_id(); }

        Value get() const
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

        void set(Value value)
        {
            arena< 1024 > buffer;
            arena_allocator< void, allocator< typename Allocator::replica_type::delta_replica_type > > allocator(buffer, values_.get_allocator().get_replica());
            typename decltype(values_)::template rebind< decltype(allocator) >::type delta(allocator, this->get_id());

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

        template < typename ValueMv > value_mv< Value, Allocator >& operator = (const ValueMv& value)
        {
            merge(value);
            return *this;
        }

        bool operator == (const Value& value) const { return get() == value; }
        bool operator == (const value_mv< Value, Allocator >& other) const { return get() == other.get(); }

    private:
        crdt::set< Value, Allocator > values_;
    };
}