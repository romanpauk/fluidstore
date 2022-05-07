#include <fluidstore/column/column.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <tuple>

BOOST_AUTO_TEST_CASE(test_index_1)
{
    column::tree_index< 1, int > t;
    t.emplace(1);
    *t.begin();
}

BOOST_AUTO_TEST_CASE(test_prefix)
{
    column::tree< int, int, int > t;
    BOOST_REQUIRE(t.size() == 0);
    
    t.emplace(0, 0, 0);
    BOOST_REQUIRE(t.size() == 1);
    BOOST_REQUIRE(t.prefix(0)->size() == 1);
    BOOST_REQUIRE(t.prefix(0, 0)->size() == 1);

    t.emplace(0, 0, 1);
    BOOST_REQUIRE(t.size() == 2);
    BOOST_REQUIRE(t.prefix(0)->size() == 2);
    BOOST_REQUIRE(t.prefix(0, 0)->size() == 2);

    t.emplace(0, 1, 0);
    BOOST_REQUIRE(t.size() == 3);
    BOOST_REQUIRE(t.prefix(0)->size() == 3);
    BOOST_REQUIRE(t.prefix(0, 1)->size() == 1);

    t.emplace(0, 1, 1);
    BOOST_REQUIRE(t.size() == 4);
    BOOST_REQUIRE(t.prefix(0)->size() == 4);
    BOOST_REQUIRE(t.prefix(0, 1)->size() == 2);

    auto&& t0 = t.prefix(0);
    BOOST_REQUIRE(t0->size() == 4);

    auto&& t00_1 = t0->prefix(0);
    BOOST_REQUIRE(t00_1->size() == 2);

    auto&& t00_2 = t.prefix(0, 0);
    BOOST_REQUIRE(t00_1 == t00_2);
}

BOOST_AUTO_TEST_CASE(test_column_iterator)
{
    column::tree< int, int, int > t;
    t.emplace(1, 1, 1);
    auto it = t.begin();
    // *it;
}