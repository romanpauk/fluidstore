#include <fluidstore/btree/btree.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(btree_insert)
{
    btree::set< int > c;
    BOOST_TEST(c.size() == 0);
    BOOST_TEST(c.empty());
    BOOST_TEST(c.find(0) == false);

    c.insert(2);
    BOOST_TEST(c.size() == 1);
    BOOST_TEST(c.find(2) == true);

    c.insert(1);
    BOOST_TEST(c.size() == 2);
    BOOST_TEST(c.find(1) == true);
}

BOOST_AUTO_TEST_CASE(btree_insert_loop)
{
    btree::set< int > c;
    for (int i = 0; i < 1000; ++i)
    {
        c.insert(i);
        BOOST_TEST(c.find(i) == true);

        // Check that the tree was not damaged by insertion
        for (int j = 0; j < i; ++j)
        {
            BOOST_TEST(c.find(j) == true);
        }
    }
}