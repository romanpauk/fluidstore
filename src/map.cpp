#include <boost/test/unit_test.hpp>

#include <sqlstl/sqlite.h>
#include <sqlstl/map.h>
#include <sqlstl/allocator.h>

BOOST_AUTO_TEST_CASE(map)
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);
    sqlstl::allocator<void> allocator(factory);

    sqlstl::map< int, int, sqlstl::allocator< std::pair< const int, int > > > map(allocator);

    BOOST_ASSERT(map.size() == 0);

    map[1] = 10;
    {
        auto it = map.find(1);
        BOOST_ASSERT((*it).first == 1);
        BOOST_ASSERT((*it).second == 10);
        BOOST_ASSERT(++it == map.end());
        BOOST_ASSERT(map.size() == 1);
    }

    map[2] = 20;
    {
        auto it = map.find(2);
        BOOST_ASSERT((*it).first == 2);
        BOOST_ASSERT((*it).second == 20);
        BOOST_ASSERT(++it == map.end());
        BOOST_ASSERT(map.size() == 2);
    }

    {
        int count = 0;
        for (auto&& value : map)
        {
            BOOST_ASSERT(value.first == ++count);
            BOOST_ASSERT(value.second == count * 10);
        }
        BOOST_ASSERT(count == 2);
    }

/*
    sqlstl::map< int, sqlstl::map< int, int, sqlstl::allocator >, sqlstl::allocator > nested(allocator, "Map2");
    nested[1][10] = 100;
    nested[2][20] = 200;

    {
        int count = 0;
        for (auto&& value : nested)
        {
            BOOST_ASSERT(value.first == ++count);
            BOOST_ASSERT((*value.second.find(count * 10)).second == count * 100);
        }
        BOOST_ASSERT(count == 2);
    }
*/
}
