#pragma once

#include <fluidstore/crdts/set2.h>

namespace crdt
{
    template < typename Value, typename Allocator, typename Tag, template <typename> typename Hook = hook_none >
    class value_mv2;

    template < typename Value, typename Allocator, template <typename> typename Hook >
        class value_mv2< Value, Allocator, tag_delta, Hook >
    {
    public:
        using allocator_type = Allocator;

        value_mv2(allocator_type& allocator)
            : values_(allocator)
        {}

        template < typename ValueMv > void merge(const ValueMv& other)
        {
            values_.merge(other.values_);
        }

        allocator_type& get_allocator()
        {
            return allocator_;
        }

    //private:
        crdt::set2< Value, Allocator, tag_delta, Hook > values_;
    };

    template < typename Value, typename Allocator, template <typename> typename Hook >
    class value_mv2< Value, Allocator, tag_state, Hook >
        : public Hook < value_mv2 < Value, Allocator, tag_delta > >
    {
    public:
        using allocator_type = Allocator;

        template < typename AllocatorT > struct rebind
        {
            using other = value_mv2< Value, AllocatorT, tag_state, Hook >;
        };

        value_mv2(allocator_type& allocator)
            : Hook < value_mv2 < Value, Allocator, tag_delta > >(allocator)
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
            
            crdt::value_mv2< Value, decltype(deltaallocator), tag_delta > delta(deltaallocator);
            values_.clear(delta.values_);
            values_.delta_insert(delta.values_, value);
            values_.merge(delta.values_);

            commit_delta(delta);
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

        allocator_type& get_allocator()
        {
            return values_.get_allocator();
        }

    private:
        crdt::set2< Value, Allocator, tag_state, Hook > values_;
    };
}