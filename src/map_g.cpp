#include <boost/test/unit_test.hpp>

#include <crdt/traits.h>
#include <crdt/value_mv.h>
#include <crdt/map_g.h>

#include <crdt/counter_g.h>

template < typename Traits, typename Allocator > void map_g_test_impl(typename Allocator allocator)
{
    crdt::map_g< int, int, Traits > map(allocator);
    BOOST_ASSERT(map.size() == 0);
    {
        auto it = map.emplace(1, 10);

        BOOST_ASSERT((*it.first == std::pair< const int, int >(1, 10)));
        BOOST_ASSERT(it.second == true);
    }
    {
        auto it = map.emplace(1, 20);
        BOOST_ASSERT((*it.first == std::pair< const int, int >(1, 10)));
        BOOST_ASSERT(it.second == false);
    }
    BOOST_ASSERT(map.size() == 1);
    BOOST_ASSERT(map.find(1) != map.end());
    map.emplace(2, 20);
    BOOST_ASSERT(map.size() == 2);
    BOOST_ASSERT(map.find(2) != map.end());

    int count = 0;
    for (auto&& value : map)
    {
        ++count;
        BOOST_ASSERT(value.first == count);
    }

    BOOST_ASSERT(count == 2);
}

BOOST_AUTO_TEST_CASE(map_g_test)
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