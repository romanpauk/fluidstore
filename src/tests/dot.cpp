#include <fluidstore/crdt/allocator.h>
#include <fluidstore/crdt/detail/dot_context.h>
#include <fluidstore/crdt/detail/dot_counters_base.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_context)
{
    typedef crdt::dot< uint64_t, uint64_t > dot;
   
    std::allocator< dot > allocator;
    crdt::dot_context< dot, crdt::tag_delta > counters;

    counters.emplace(allocator, dot{ 2u, 1u });
    counters.emplace(allocator, dot{ 2u, 3u });
    counters.collapse(allocator);
    BOOST_TEST(counters.has(dot{ 2, 1 }));
    BOOST_TEST(counters.has(dot{ 2, 3 }));
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, dot{ 2u, 2u });
    counters.collapse(allocator);
    BOOST_TEST(counters.has(dot{ 2, 3 }));
    BOOST_TEST(counters.size() == 1);

    counters.emplace(allocator, dot{ 1u, 2u });
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    counters.emplace(allocator, dot{ 1u, 1u });
    counters.collapse(allocator);
    BOOST_TEST(counters.size() == 2);
    BOOST_TEST(!counters.has(dot{ 1, 1 }));

    // TODO: this will collapse without waiting for 1.

    counters.emplace(allocator, dot{ 3u, 3u });
    {
        crdt::dot_context< crdt::dot< uint64_t, uint64_t >, crdt::tag_delta > counters2;
        counters2.emplace(allocator, dot{ 3u, 2u });
        counters2.emplace(allocator, dot{ 3u, 4u });
        counters.merge(allocator, counters2);
        counters.collapse(allocator);

        counters2.clear(allocator);
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST(counters.has(dot{ 3, 4 }));

    counters.clear(allocator);
}

BOOST_AUTO_TEST_CASE(dot_context_move)
{
    typedef crdt::dot< uint64_t, uint64_t > dot;

    std::allocator< dot > allocator;
    crdt::dot_context< dot, crdt::tag_delta > counters;

    //static_assert(std::is_move_assignable_v< decltype(counters) >);
    static_assert(std::is_move_constructible_v< decltype(counters) >);
}

BOOST_AUTO_TEST_CASE(dot_counters_base)
{
    typedef crdt::dot< uint64_t, uint64_t > dot;

    std::allocator< dot > allocator;
    crdt::dot_counters_base< uint64_t, crdt::tag_delta > counters;

    //static_assert(std::is_move_assignable_v< decltype(counters) >);
    static_assert(std::is_move_constructible_v< decltype(counters) >);
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(dot_context_sizeof)
{
    PRINT_SIZEOF(crdt::dot_context< crdt::dot< uint64_t, uint64_t >, crdt::tag_delta >);
}