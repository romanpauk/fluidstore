#include <fluidstore/btree/btree.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(btree_insert)
{
    btree::set< int > c;
    BOOST_TEST(c.size() == 0);
    BOOST_TEST(c.empty());

    c.insert(2);
    BOOST_TEST(c.size() == 1);
    c.insert(1);
    BOOST_TEST(c.size() == 2);

    for (int i = 3; i < 100; ++i)
    {
        c.insert(i);
    }

}