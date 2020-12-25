#pragma once

namespace crdt
{
    template < typename Value, typename Allocator > class value
    {
    public:
        typedef Allocator allocator_type;

        template < typename AllocatorT > struct rebind
        {
            // TODO: this can also be recursive in Value... sometimes.
            typedef value< Value, AllocatorT > type;
        };

        value(allocator_type)
            : value_()
        {}

        value(allocator_type, const Value& value)
            : value_(value)
        {}

        template < typename ValueT > void merge(const ValueT& other)
        {
            value_ = other.value_;
        }

        bool operator == (const Value& value) const { return value_ == value; }
        bool operator == (const value< Value, Allocator >& other) const { return value_ == other.value_; }

    private:
        Value value_;
    };
}