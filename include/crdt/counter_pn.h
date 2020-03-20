#pragma once

#include <crdt/allocator.h>
#include <crdt/counter_g.h>

namespace crdt
{
    template < typename T, typename Node, typename Traits > class counter_pn
    {
    public:
        template < typename Allocator > counter_pn(Allocator&& allocator)
            : inc_(Allocator(allocator, "inc"))
            , dec_(Allocator(allocator, "dec"))
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
}