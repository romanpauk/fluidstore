#include <boost/test/unit_test.hpp>

#include <crdt/value_mv.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void value_mv_test_impl(typename Allocator allocator)
{
    crdt::value_mv< int, Traits > value(Allocator(allocator, "value_mv"));
    value = 1;
    BOOST_ASSERT(value == 1);
    value = 2;
    BOOST_ASSERT(value == 2);

    BOOST_ASSERT((std::uses_allocator_v< crdt::value_mv< int, Traits >, Allocator >));
    BOOST_ASSERT((std::uses_allocator_v< crdt::value_mv< int, Traits >, std::allocator< void > >));
}

BOOST_AUTO_TEST_CASE(value_mv_test)
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);

        value_mv_test_impl< crdt::traits< crdt::sqlite > >(sqlstl::allocator< void >(factory));
    }

    {
        crdt::allocator<void> allocator;
        value_mv_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}