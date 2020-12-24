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
            // TODO: those are two merges, should be one. It is easy for this case when the container is the same,
            // but in general this should be supported for different container types. And that is what replica does now, 
            // it gathers different delta types for later processing.
            
            values_.clear();
            values_.insert(value);
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

    private:
        crdt::set< Value, Allocator > values_;
    };
}