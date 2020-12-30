#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/dot_context.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_context)
{
    std::allocator< void > allocator;
    crdt::dot_context< uint64_t, uint64_t, std::allocator< char > > counters(allocator);
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
        crdt::dot_context< uint64_t, uint64_t, std::allocator< char > > counters2(allocator);
        counters2.emplace(3, 2);
        counters2.emplace(3, 4);
        counters.merge(counters2, true);
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST((counters.find(dot(3, 4)) != counters.end()));
}
