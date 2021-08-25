#include <fluidstore/btree/btree.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(btree_insert)
{
    btree::set< int > c;
    BOOST_TEST(c.size() == 0);
    BOOST_TEST(c.empty());
    BOOST_TEST((c.find(0) == c.end()));

    {
        auto pairb = c.insert(2);
        BOOST_TEST(pairb.second == true);
        BOOST_TEST(*pairb.first == 2);
        BOOST_TEST((c.find(2) == pairb.first));
        BOOST_TEST((c.find(2) != c.end()));

        BOOST_TEST(c.size() == 1);
        BOOST_TEST(c.insert(2).second == false);
        BOOST_TEST(c.size() == 1);
    }

    c.insert(1);
    BOOST_TEST(c.size() == 2);
    BOOST_TEST((c.find(1) != c.end()));
}

BOOST_AUTO_TEST_CASE(btree_range_for)
{
    btree::set< int > c;
    for (int i = 0; i < 30; ++i)
    {
        c.insert(i);
    }

    int i = 0;
    for (auto& value : c)
    {
        BOOST_REQUIRE(value == i++);
    }

    BOOST_REQUIRE(i == c.size());
}

BOOST_AUTO_TEST_CASE(btree_insert_loop)
{
    btree::set< int > c;
    for (int i = 0; i < 1000; ++i)
    {
        c.insert(i);
        BOOST_REQUIRE(*c.find(i) == i);

        // Check that the tree was not damaged by insertion
        for (int j = 0; j < i; ++j)
        {
            BOOST_REQUIRE(*c.find(j) == j);
        }
    }
}


BOOST_AUTO_TEST_CASE(btree_erase)
{
    for (int i = 0; i < 100; ++i)
    {
        std::cerr << "i: " << i << std::endl;

        btree::set< int > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert(j);
        }

        for (int j = 0; j < i; ++j)
        {
            c.erase(j);
            BOOST_REQUIRE((c.find(j) == c.end()));

            for (int k = j + 1; k < i; ++k)
            {
                BOOST_REQUIRE(c.find(k) != c.end());
            }
        }
    }

    for (int i = 0; i < 100; ++i)
    {
        std::cerr << "j: " << i << std::endl;

        btree::set< int > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert(j);
        }

        for (int j = i - 1; j > 0; --j)
        {
            c.erase(j);
            if (c.find(j) != c.end())
            {
                int a(1);
            }

            BOOST_REQUIRE((c.find(j) == c.end()));

            for (int k = j - 1; k > 0; --k)
            {
                if (c.find(k) == c.end())
                {
                    int a(1);
                }

                BOOST_REQUIRE(c.find(k) != c.end());
            }
        }
    }
}