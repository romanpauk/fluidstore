#define NOMINMAX

// TODO: fix
#undef _ENFORCE_MATCHING_ALLOCATORS
#define _ENFORCE_MATCHING_ALLOCATORS 0

#include <boost/test/unit_test.hpp>

#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/noncopyable.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/value.h>
#include <fluidstore/crdts/value_mv.h>

#include <chrono>

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
    crdt::traits::id_sequence_type sequence;
    crdt::traits::replica_type replica(0, sequence);
    crdt::traits::allocator_type alloc(replica);
    crdt::map< int, crdt::value_mv< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    BOOST_TEST((map.find(0) == map.end()));
    // map.insert(1, 1);
    // BOOST_TEST(map.find(1) != map.end());
    // map.erase(1);
    // BOOST_TEST(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
    crdt::traits::id_sequence_type sequence;
    crdt::traits::replica_type replica(0, sequence);
    crdt::traits::allocator_type alloc(replica);
    crdt::map< 
        int, 
        crdt::map< 
            int, 
            crdt::value_mv< int, decltype(alloc) >, decltype(alloc)
        >, 
        decltype(alloc)
    > map(alloc, { 0, 1 });

    //BOOST_TEST(map.find(0) == false);
    //map.insert(1, 1);
    //BOOST_TEST(map.find(1) == true);
    //map.erase(1);
    //BOOST_TEST(map.find(1) == false);
}

template < typename Fn > double measure(Fn fn)
{
    using namespace std::chrono;

    auto begin = high_resolution_clock::now();
    fn();
    auto end = high_resolution_clock::now();
    return duration_cast< duration<double> >(end - begin).count();
}

#if !defined(_DEBUG)
BOOST_AUTO_TEST_CASE(dot_test_set_insert_performance)
{
    /*
    auto x = measure([]
    {
        crdt::traits::allocator_type alloc(0);
        crdt::set< size_t, crdt::traits > stdset(alloc);
        for (size_t i = 0; i < 1000000; ++i)
        {
            stdset.test(i);
        }
    });
    */

    #define Outer 10000
    #define Inner 100

    auto t1 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            std::set< size_t > set;
            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });

    auto t2 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            crdt::traits::replica_type replica(0);
            crdt::traits::allocator_type alloc(replica);
            crdt::set< size_t, crdt::traits > set(alloc);
            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });

    std::cerr << "std::set " << t1 << ", crdt::set " << t2 << ", slowdown " << t2/t1 << std::endl;
}

#endif