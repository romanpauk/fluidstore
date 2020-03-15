#include <crdt/counter_g.h>
#include <crdt/traits.h>

template < typename Traits > void counter_g_test_impl(typename Traits::Allocator& allocator)
{
    crdt::counter_g< int, int, Traits > counter(allocator, "counter_g");
    assert(counter.value() == 0);
    counter.add(1, 1);
    counter.add(2, 3);
    assert(counter.value() == 4);
}

void counter_g_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::allocator allocator(db);

        counter_g_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator allocator;
        counter_g_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}