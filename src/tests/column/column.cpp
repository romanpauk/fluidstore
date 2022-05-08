#include <fluidstore/column/column.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <tuple>

BOOST_AUTO_TEST_CASE(test_index_1)
{
    column::detail::set< 1, int > t;
    t.emplace(1);
    *t.begin();
}


BOOST_AUTO_TEST_CASE(test_get_tuple)
{
    //std::tuple< int > t1 = column::detail::value_type_tuple(1);
    //std::tuple< int, int > t2 = column::detail::value_type_tuple(std::make_pair(1, 1));
    //std::tuple< int, int, int > t3 = column::detail::value_type_tuple(std::make_pair(1, std::make_tuple(1, 1)));
}

BOOST_AUTO_TEST_CASE(test_prefix)
{
    column::set< int, int, int > t;
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

template < typename Tf, typename Ts, typename Uf, typename Us > bool compare_pairs(const std::pair< Tf, Ts >& lhs, const std::pair< Uf, Us >& rhs)
{
    return std::make_tuple(lhs.first, lhs.second) == std::make_tuple(rhs.first, rhs.second);
}

BOOST_AUTO_TEST_CASE(test_iterator_dereference)
{
    column::set< int, int, int > t;
    t.emplace(1, 1, 1);
    t.emplace(1, 1, 2);
    t.emplace(1, 2, 1);

    {
        auto it = t.begin();
        BOOST_REQUIRE(compare_pairs((*it), std::make_pair(1, std::make_tuple(1, 1))));
    }

    {
        auto it = t.prefix(1)->begin();
        BOOST_REQUIRE(compare_pairs((*it), std::make_pair(1, std::make_tuple(1))));
    }

    {
        auto it = t.prefix(1, 1)->begin();
        BOOST_REQUIRE((*it) == 1);
    }
}

BOOST_AUTO_TEST_CASE(test_iterator_distance)
{
    column::set< int, int, int > t;
    BOOST_REQUIRE(t.begin() == t.end());
    BOOST_REQUIRE(std::distance(t.begin(), t.end()) == 0);

    t.emplace(1, 1, 1);
    BOOST_REQUIRE(++t.begin() == t.end());
    BOOST_REQUIRE(std::distance(t.begin(), t.end()) == 1);
    
    t.emplace(1, 1, 2);
    BOOST_REQUIRE(std::distance(t.begin(), t.end()) == 2);
    BOOST_REQUIRE(std::distance(t.prefix(1, 1)->begin(), t.prefix(1, 1)->end()) == 2);

    t.emplace(1, 2, 1);
    BOOST_REQUIRE(std::distance(t.begin(), t.end()) == 3);
    BOOST_REQUIRE(std::distance(t.prefix(1, 2)->begin(), t.prefix(1, 2)->end()) == 1);
}

BOOST_AUTO_TEST_CASE(test_pair)
{
    std::tuple< const int, int > t1;
    std::tuple< int, int > t2;
    // t1 == t2;

    std::pair< const int, int > p1;
    std::pair< int, int > p2;
    // p1 == p2;
}