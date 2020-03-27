#include <boost/test/unit_test.hpp>

#include <sqlstl/set.h>

BOOST_AUTO_TEST_CASE(set)
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);
    sqlstl::allocator<void> allocator(factory);

    sqlstl::set< int, sqlstl::allocator< int > > set(allocator);

    BOOST_ASSERT(set.size() == 0);

    {
        auto it = set.insert(1);
        BOOST_ASSERT(it.first == set.find(1));
        BOOST_ASSERT(*it.first == 1);
        BOOST_ASSERT(it.first != set.find(2));
        BOOST_ASSERT(it.second == true);
    }

    {
        auto it = set.insert(1);
        BOOST_ASSERT(it.second == false);
        BOOST_ASSERT(*it.first == 1);
    }

    BOOST_ASSERT(set.size() == 1);
    
    {
        auto it = set.insert(2);
        BOOST_ASSERT(set.find(2) == it.first);
    }

    BOOST_ASSERT(set.size() == 2);

    {
        int count = 0;
        for (auto&& value : set)
        {
            BOOST_ASSERT(value == ++count);
        }
        BOOST_ASSERT(count == 2);
    }
}

