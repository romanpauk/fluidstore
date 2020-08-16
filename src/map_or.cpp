#include <boost/test/unit_test.hpp>

#include <crdt/map_or.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void map_or_test_impl(typename Allocator allocator)
{
    crdt::map_or< int, int, Traits > map(Allocator(allocator, "map"));
    BOOST_ASSERT(map.size() == 0);
    {
        auto it = map.insert(1, 10);
        // BOOST_ASSERT(*it.first == std::make_pair(1, 10));
        BOOST_ASSERT(it.second == true);
    }
    {
        auto it = map.insert(1, 20);
        // BOOST_ASSERT(*it.first == std::make_pair(1, 10));
        BOOST_ASSERT(it.second == false);
    }
    BOOST_ASSERT(map.size() == 1);
    BOOST_ASSERT(map.find(1) != map.end());
    map.insert(2, 20);
    BOOST_ASSERT(map.size() == 2);
    BOOST_ASSERT(map.find(2) != map.end());

    int count = 0;
    for (auto&& value : map)
    {
        ++count;
        BOOST_ASSERT(value == count);
    }

    BOOST_ASSERT(count == 2);

    map.erase(2);
    BOOST_ASSERT(map.find(2) == map.end());

    count = 0;
    for (auto&& value : map)
    {
        ++count;
        BOOST_ASSERT(value == count);
    }
    BOOST_ASSERT(count == 1);
}

BOOST_AUTO_TEST_CASE(map_or_test)
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);
        map_or_test_impl< crdt::traits< crdt::sqlite > >(sqlstl::allocator<void>(factory));
    }

    {
        crdt::allocator<void> allocator;
        map_or_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}