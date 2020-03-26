#include <crdt/map_g.h>
#include <crdt/value_mv.h>
#include <crdt/counter_g.h>

#include <crdt/traits.h>

template < typename Traits, typename Allocator > void map_g_test_impl(typename Allocator allocator)
{
    crdt::map_g< int, crdt::value_mv< int, Traits >, Traits > map(allocator);
    map[1] = 1;
    assert(map.find(1) != map.end());
    assert(map[1] == 1);
}

void map_g_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);
        sqlstl::allocator<void> allocator(factory);
        map_g_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator< void > allocator;
        map_g_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}