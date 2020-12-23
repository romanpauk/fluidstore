#pragma once

#include <fluidstore/crdts/set.h>

namespace crdt
{
    template < typename Value, typename Allocator, typename Replica > class value_mv
        : public Replica::template hook< value_mv< Value, Allocator, Replica > >
    {
        typedef value_mv< Value, Allocator, Replica > value_mv_type;
        template < typename Value, typename Allocator, typename Replica > friend class value_mv;

    public:
        typedef Allocator allocator_type;

        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value_mv< Value, AllocatorT, ReplicaT > type;
        };

        value_mv(allocator_type allocator)
            : Replica::template hook< value_mv_type >(allocator.get_replica())
            , values_(allocator)

        {}

        value_mv(allocator_type allocator, typename Replica::id_type id)
            : Replica::template hook< value_mv_type >(allocator.get_replica(), id)
            , values_(allocator)
        {}

        value_mv(allocator_type allocator, typename Replica::id_type id, const Value& value)
            : Replica::template hook< value_mv_type >(allocator.get_replica(), id)
            , values_(allocator)
        {
            set(value);
        }

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

        bool operator == (const Value& value) const { return get() == value; }

    private:
        crdt::set< Value, Allocator, Replica > values_;
    };
}