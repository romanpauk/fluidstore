#include <crdt/value_mv.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void value_mv_test_impl(typename Allocator allocator)
{
    crdt::value_mv< int, Traits > value(Allocator(allocator, "value_mv"));
    value = 1;
    assert(value == 1);
    value = 2;
    assert(value == 2);
}

void value_mv_test()
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