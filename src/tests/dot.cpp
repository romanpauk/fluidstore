#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/dot_context.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_context)
{
    std::allocator< void > allocator;
    crdt::dot_context< uint64_t, uint64_t, std::allocator< char >, crdt::tag_delta > counters(allocator);
    typedef crdt::dot< uint64_t, uint64_t > dot;

    counters.emplace(2, 1);
    counters.emplace(2, 3);
    counters.collapse();
    BOOST_TEST((counters.find(dot(2, 1)) != counters.end()));
    BOOST_TEST((counters.find(dot(2, 3)) != counters.end()));
    BOOST_TEST(counters.size() == 2);
    counters.emplace(2, 2);
    counters.collapse();
    BOOST_TEST((counters.find(dot(2, 3)) != counters.end()));
    BOOST_TEST(counters.size() == 1);

    counters.emplace(1, 2);
    counters.collapse();
    BOOST_TEST(counters.size() == 2);
    counters.emplace(1, 1);
    counters.collapse();
    BOOST_TEST(counters.size() == 2);
    BOOST_TEST((counters.find(dot(1, 1)) == counters.end()));

    // TODO: this will collapse without waiting for 1.

    counters.emplace(3, 3);
    {
        crdt::dot_context< uint64_t, uint64_t, std::allocator< char >, crdt::tag_delta > counters2(allocator);
        counters2.emplace(3, 2);
        counters2.emplace(3, 4);
        counters.merge(counters2);
        counters.collapse();
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST((counters.find(dot(3, 4)) != counters.end()));
}

BOOST_AUTO_TEST_CASE(dot_context2)
{
    typedef crdt::dot< uint64_t, uint64_t > dot;

    std::allocator< dot > allocator;
    crdt::dot_context2< uint64_t, uint64_t, crdt::tag_delta > counters;

    counters.emplace(allocator, 2, 1);
    counters.emplace(allocator, 2, 3);
    counters.collapse(allocator);
    BOOST_TEST((counters.find(dot(2, 1)) != counters.end()));
    BOOST_TEST((counters.find(dot(2, 3)) != counters.end()));
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, 2, 2);
    counters.collapse(allocator);
    BOOST_TEST((counters.find(dot(2, 3)) != counters.end()));
    BOOST_TEST(counters.size() == 1);

    counters.emplace(allocator, 1, 2);
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, 1, 1);
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    BOOST_TEST((counters.find(dot(1, 1)) == counters.end()));

    // TODO: this will collapse without waiting for 1.

    counters.emplace(allocator, 3, 3);
    {
        crdt::dot_context2< uint64_t, uint64_t, crdt::tag_delta > counters2;
        counters2.emplace(allocator, 3, 2);
        counters2.emplace(allocator, 3, 4);
        counters.merge(allocator, counters2);
        counters.collapse(allocator);

        counters2.clear(allocator);
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST((counters.find(dot(3, 4)) != counters.end()));

    counters.clear(allocator);
}
