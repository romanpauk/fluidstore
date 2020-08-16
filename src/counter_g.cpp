#include <boost/test/unit_test.hpp>

#include <crdt/counter_g.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void counter_g_test_impl(typename Allocator allocator)
{
    crdt::counter_g< int, int, Traits > counter(Allocator(allocator, "counter"));
    BOOST_ASSERT(counter.value() == 0);
    counter.increment(1, 1);
    counter.increment(2, 3);
    BOOST_ASSERT(counter.value() == 4);
}

BOOST_AUTO_TEST_CASE(counter_g_test)
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);
        sqlstl::allocator<void> allocator(factory);
        counter_g_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator< void > allocator;
        counter_g_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}