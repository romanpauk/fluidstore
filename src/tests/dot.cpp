#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/dot_context.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_context)
{
    typedef crdt::dot< uint64_t, uint64_t > dot;
    static_assert(std::is_trivially_copyable_v< dot >);

    std::allocator< dot > allocator;
    crdt::dot_context< dot, crdt::tag_delta > counters;

    counters.emplace(allocator, 2u, 1u);
    counters.emplace(allocator, 2u, 3u);
    counters.collapse(allocator);
    BOOST_TEST((counters.find(dot{ 2, 1 }) != counters.end()));
    BOOST_TEST((counters.find(dot{ 2, 3 }) != counters.end()));
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, 2u, 2u);
    counters.collapse(allocator);
    BOOST_TEST((counters.find(dot{ 2, 3 }) != counters.end()));
    BOOST_TEST(counters.size() == 1);

    counters.emplace(allocator, 1u, 2u);
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, 1u, 1u);
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    BOOST_TEST((counters.find(dot{ 1, 1 }) == counters.end()));

    // TODO: this will collapse without waiting for 1.

    counters.emplace(allocator, 3u, 3u);
    {
        crdt::dot_context< crdt::dot< uint64_t, uint64_t >, crdt::tag_delta > counters2;
        counters2.emplace(allocator, 3u, 2u);
        counters2.emplace(allocator, 3u, 4u);
        counters.merge(allocator, counters2);
        counters.collapse(allocator);

        counters2.clear(allocator);
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST((counters.find(dot{ 3, 4 }) != counters.end()));

    counters.clear(allocator);
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(dot_context_sizeof)
{
    PRINT_SIZEOF(crdt::dot_context< crdt::dot< uint64_t, uint64_t >, crdt::tag_delta >);
}