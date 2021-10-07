#include <fluidstore/btree/btree.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(perf_test)
{
    for (size_t i = 0; i < 1000000; ++i)
    {
        crdt::arena< 65536 > arena;
        crdt::arena_allocator< void > arenaallocator(arena);
        btree::set< int, std::less< int >, decltype(arenaallocator) > c(arenaallocator);

        for (size_t j = 0; j < 100; ++j)
        {
            c.insert(~j);
        }
    }
}
