#pragma once

#include <crdt/allocator.h>
#include <crdt/counter_g.h>

namespace crdt
{
    template < typename T, typename Node, typename Traits > class counter_pn
    {
        template < typename T, typename Node, typename Traits > friend class counter_pn;

    public:
        typedef typename Traits::template Allocator<void> allocator_type;

        counter_pn(allocator_type allocator)
            : allocator_(allocator)
            , inc_(allocator_type(allocator, "inc"))
            , dec_(allocator_type(allocator, "dec"))
        {}

        void increment(const Node& node, T value)
        {
            inc_.increment(node, value);
        }

        void decrement(const Node& node, T value)
        {
            dec_.increment(node, value);
        }

        T value() const
        {
            return inc_.value() - dec_.value();
        }

        T value(const Node& node) const
        {
            return inc_.value(node) - dec_.value(node);
        }

        template < typename Counter > void merge(const Counter& other)
        {
            inc_.merge(other.inc_);
            dec_.merge(other.dec_);
        }

        template < typename Counter > bool operator == (const Counter& other) const
        {
            return std::tie(inc_, dec_) == std::tie(other.inc_, other.dec_);
        }

        auto get_allocator() const { return allocator_; }

    private:
        allocator_type allocator_;
        counter_g< T, Node, Traits > inc_, dec_;
    };

    template < typename T, typename Node, typename StateTraits, typename DeltaTraits > class counter_pn_delta
        : public delta_crdt_base
    {
        typedef counter_pn< T, Node, StateTraits > state_container_type;
        typedef counter_pn< T, Node, DeltaTraits > delta_container_type;

    public:
        counter_pn_delta(state_container_type& state_container)
            : state_container_(state_container)
        {}

        void increment(const Node& node, T value)
        {
            delta_container_.increment(node, state_container_.value(node) + value);
        }

        void decrement(const Node& node, T value)
        {
            delta_container_.decrement(node, state_container_.value(node) + value);
        }

        void commit() override
        {
            state_container_.merge(delta_container_);
        }

    private:
        state_container_type& state_container_;
        delta_container_type delta_container_;
    };
}