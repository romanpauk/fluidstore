#pragma once

namespace crdt
{
    template < typename Value, typename Allocator, typename Replica, typename Counter > class value_mv_base
        : public Replica::template hook< value_mv_base< Value, Allocator, Replica, Counter > >
    {
        typedef value_mv_base< Value, Allocator, Replica, Counter > value_mv_base_type;

        template < typename Value, typename Allocator, typename Replica, typename Counter > friend class value_mv_base;

    public:
        typedef Allocator allocator_type;

        template < typename AllocatorT, typename ReplicaT > struct rebind
        {
            // TODO: this can also be recursive in Value
            typedef value_mv_base< Value, AllocatorT, ReplicaT, Counter > type;
        };

        value_mv_base(allocator_type allocator)
            : Replica::template hook< value_mv_base_type >(allocator.get_replica())
            , values_(allocator)

        {}

        value_mv_base(allocator_type allocator, typename Replica::id_type id)
            : Replica::template hook< value_mv_base_type >(allocator.get_replica(), id)
            , values_(allocator)
        {}

        value_mv_base(allocator_type allocator, typename Replica::id_type id, const Value& value)
            : Replica::template hook< value_mv_base_type >(allocator.get_replica(), id)
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

        template < typename ValueMvBase > void merge(const ValueMvBase& other)
        {
            values_.merge(other.values_);
        }

        value_mv_base& operator = (const Value& value)
        {
            set(value);
            return *this;
        }

        bool operator == (const Value& value) const { return get() == value; }

    private:
        crdt::set_base< Value, Allocator, Replica, Counter > values_;
    };

    template < typename Value, typename Traits > class value_mv
        : public value_mv_base< Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type >
    {
        typedef value_mv_base< Value, typename Traits::allocator_type, typename Traits::replica_type, typename Traits::counter_type > value_mv_base_type;

    public:
        typedef typename Traits::allocator_type allocator_type;

        value_mv(allocator_type allocator)
            : value_mv_base_type(allocator)
        {}

        value_mv(allocator_type allocator, typename Traits::id_type id)
            : value_mv_base_type(allocator, id)
        {}
    };
}