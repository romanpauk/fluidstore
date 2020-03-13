#pragma once

namespace crdt
{
    template < typename T, typename Node, typename Traits > class counter_pn
    {
    public:
        counter_pn(typename Traits::Allocator& allocator = allocator::static_allocator(), const std::string& name = "")
            : inc_(allocator, name + ".inc")
            , dec_(allocator, name + ".dec")
        {}

        void add(const Node& node, T value)
        {
            inc_.add(node, value);
        }

        void sub(const Node& node, T value)
        {
            dec_.add(node, value);
        }

        T value() const
        {
            return inc_.value() - dec_.value();
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

        // private:
        counter_g< T, Node, Traits > inc_, dec_;
    };

    template < typename T, typename Node, typename DeltaTraits, typename StateTraits = DeltaTraits > class delta_counter_pn
        : public counter_pn< T, Node, StateTraits >
    {
    public:
        delta_counter_pn(typename StateTraits::Allocator& allocator, const std::string& name = "")
            : counter_pn< T, Node, StateTraits >(allocator, name)
        {}

        counter_pn< T, Node, DeltaTraits > add(const Node& node, T value)
        {
            counter_pn< T, Node, DeltaTraits > delta;
            delta.add(node, value);
            this->merge(delta);
            return delta;
        }

        counter_pn< T, Node, DeltaTraits > sub(const Node& node, T value)
        {
            counter_pn< T, Node, DeltaTraits > delta;
            delta.sub(node, value);
            this->merge(delta);
            return delta;
        }
    };
}