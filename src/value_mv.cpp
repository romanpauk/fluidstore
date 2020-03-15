#include <crdt/value_mv.h>
#include <crdt/traits.h>

template < typename Traits > void value_mv_test_impl(typename Traits::Allocator& allocator)
{
    crdt::value_mv< int, Traits > value(allocator, "value");
    value = 1;
    assert(value == 1);
    value = 2;
    assert(value == 2);
}

void value_mv_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::allocator allocator(db);

        value_mv_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator allocator;
        value_mv_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}