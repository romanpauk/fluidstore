#define NOMINMAX

// TODO: fix
#undef _ENFORCE_MATCHING_ALLOCATORS
#define _ENFORCE_MATCHING_ALLOCATORS 0

#include <boost/test/unit_test.hpp>

#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/noncopyable.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/delta_replica.h>
#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/value.h>
#include <fluidstore/crdts/value_mv.h>

#include <chrono>

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
        counters.merge(counters2);
    }
    BOOST_TEST(counters.size() == 3);
    BOOST_TEST((counters.find(dot(3, 4)) != counters.end()));
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> alloc(replica);
    crdt::map< int, crdt::value_mv< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    BOOST_TEST((map.find(0) == map.end()));
    // map.insert(1, 1);
    // BOOST_TEST(map.find(1) != map.end());
    // map.erase(1);
    // BOOST_TEST(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> alloc(replica);
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

typedef crdt::allocator < crdt::replica<> > delta_allocator_type;

struct visitor;
typedef crdt::delta_replica< uint64_t, uint64_t, uint64_t, crdt::arena_allocator< void, delta_allocator_type >, visitor > replica_type;

struct visitor
{
    visitor(replica_type& r)
        : replica_(r)
    {}

    template < typename T > void visit(const T& instance)
    {
        // replica_.merge(instance);
    }

    replica_type& replica_;
};

BOOST_AUTO_TEST_CASE(dot_test_set_insert_performance)
{
    #define Outer 1000
    #define Inner 1000

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
            crdt::id_sequence<> sequence;
            crdt::replica<> replica(0, sequence);
            crdt::allocator<> alloc(replica);
            crdt::set< size_t, decltype(alloc) > set(alloc, { 0,1 });
            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });

    auto t3 = measure([]
    {
        crdt::id_sequence<> sequence;
        
        crdt::arena< 32768 > arena;
        crdt::replica<> delta_replica(1, sequence);

        replica_type replica(1, sequence, [&]
        {
            return crdt::arena_allocator< void, crdt::allocator< crdt::replica<> > >(arena, delta_replica);
        });

        for (size_t x = 0; x < Outer; ++x)
        {            
            crdt::allocator< replica_type > allocator(replica);

            crdt::set< size_t, decltype(allocator) > set(allocator, { 0, 1 });

            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }

            visitor v(replica);
            replica.visit(v);
            replica.clear();
        }
    });

    std::cerr << "std::set " << t1 << std::endl;
    std::cerr << "crdt::set " << t2 << " slowdown " << t2 / t1 << std::endl;
    std::cerr << "crdt::aggregating_set " << t3 << " slowdown " << t3 / t1 << std::endl;

}

#endif