#include <crdt/counter_pn.h>
#include <crdt/traits.h>

template < typename Traits > void counter_pn_test_impl(typename Traits::Allocator& allocator)
{
    crdt::counter_pn< int, int, Traits > counter(allocator, "counter_pn");
    assert(counter.value() == 0);
    counter.add(1, 1);
    counter.add(2, 3);

    assert(counter.value() == 4);

    counter.sub(1, 2);
    counter.sub(3, 3);

    assert(counter.value() == -1);
}

void counter_pn_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::allocator allocator(db);

        counter_pn_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator allocator;
        counter_pn_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}