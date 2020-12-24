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

BOOST_AUTO_TEST_CASE(dot_set_test)
{
    crdt::traits::replica_type replica(0);
    crdt::traits::allocator_type alloc(replica);
    crdt::set< int, decltype(alloc) > set(alloc, { 0, 0 });
    BOOST_TEST((set.find(0) == set.end()));
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());

    set.insert(1);
    BOOST_TEST((set.find(1) != set.end()));
    BOOST_TEST((*set.find(1) == 1));

    BOOST_TEST(set.size() == 1);
    BOOST_TEST(!set.empty());

    set.erase(1);
    BOOST_TEST((set.find(1) == set.end()));
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());

    set.insert(1);
    BOOST_TEST(!set.empty());
    set.clear();
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());
}

BOOST_AUTO_TEST_CASE(dot_set_replica_test)
{
    crdt::traits::replica_type replica1(1);
    crdt::traits::allocator_type alloc1(replica1);
    crdt::set< int, decltype(alloc1) > set1(alloc1, { 1, 1 });

    crdt::traits::replica_type replica2(2);
    crdt::traits::allocator_type alloc2(replica2);
    crdt::set< int, decltype(alloc2) > set2(alloc2, { 2, 1 });

    set1.insert(1);
    set2.insert(2);

    set1.merge(set2);
    BOOST_TEST(set1.size() == 2);
    set2.merge(set1);
    BOOST_TEST(set2.size() == 2);
    set2.erase(1);
    set1.merge(set2);
    BOOST_TEST(set1.size() == 1);
    set2.clear();
    set1.merge(set2);
    BOOST_TEST(set1.size() == 0);
}

BOOST_AUTO_TEST_CASE(dot_map_test)
{
    crdt::traits::replica_type replica(0);
    crdt::traits::allocator_type alloc(replica);
    crdt::map< int, crdt::value< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    BOOST_TEST((map.find(0) == map.end()));
    
    //map.insert(1, crdt::value< int, crdt::traits >(alloc, 11));
    //BOOST_TEST((map.find(1) != map.end()));
    //BOOST_TEST(((*map.find(1)).first == 1));
    //BOOST_TEST(((*map.find(1)).second == 11));
    //map.erase(1);
    //BOOST_TEST((map.find(1) == map.end()));
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
    crdt::traits::replica_type replica(0);
    crdt::traits::allocator_type alloc(replica);
    crdt::map< int, crdt::set< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    BOOST_TEST((map.find(0) == map.end()));
    //map.insert(1, );
    //BOOST_TEST(map.find(1) == true);
    //map.erase(1);
    //BOOST_TEST(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(value_mv_test)
{
    crdt::traits::replica_type replica(0);
    crdt::traits::allocator_type alloc(replica);
    crdt::value_mv< int, decltype(alloc) > value(alloc);
    BOOST_TEST(value.get() == int());
    // value = 1;
    value.set(1);
    BOOST_TEST(value.get() == 1);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
    crdt::traits::replica_type replica(0);
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
    crdt::traits::replica_type replica(0);
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

BOOST_AUTO_TEST_CASE(empty_replica)
{
    typedef crdt::replica< uint64_t, uint64_t, uint64_t > replica_type;
    replica_type replica(0);
    crdt::allocator< replica_type > allocator(replica);
    typedef crdt::traits_base< replica_type > traits;

    crdt::set< int, decltype(allocator) > set(allocator, { 0, 0 });
}

struct visitor;
typedef crdt::aggregating_replica< uint64_t, uint64_t, uint64_t, crdt::instance_registry< std::pair< uint64_t, uint64_t > >, visitor > replica_type;

struct visitor
{
    visitor(replica_type& r)
        : replica_(r)
    {}

    template < typename T > void visit(const T& instance) 
    {
        replica_.merge(instance);
    }

    replica_type& replica_;
};

BOOST_AUTO_TEST_CASE(aggregating_replica)
{
    typedef crdt::traits_base< replica_type > traits;

    traits::replica_type replica1(1);
    traits::allocator_type allocator1(replica1);

    traits::replica_type replica2(2);
    traits::allocator_type allocator2(replica2);

    crdt::set< int, decltype(allocator1) > set1(allocator1, { 0, 1 });
    set1.insert(1);
    set1.insert(2);
    crdt::set< double, decltype(allocator1) > set11(allocator1, { 0, 2 });
    set11.insert(1.1);
    set11.insert(2.2);
    crdt::set< std::string, decltype(allocator1) > setstr1(allocator1, { 0, 3 });
    setstr1.insert("Hello");

    crdt::map< int, crdt::value_mv< int, decltype(allocator1) >, decltype(allocator1) > map1(allocator1, { 0, 4 });
    //map1[1] = 2;
    //map1[1] = 3;
    // TODO: for this we need the order solved ^^^

    crdt::map< int, crdt::map< int, crdt::value_mv< int, decltype(allocator1) >, decltype(allocator1) > , decltype(allocator1) > map11(allocator1, { 0, 5 });
    // crdt::map< int, crdt::value_mv< crdt::set< int, decltype(allocator1) >, decltype(allocator1) >, decltype(allocator1) > map3(allocator1, { 0, 6 });

    crdt::set< int, decltype(allocator2) > set2(allocator2, { 0, 1 });
    crdt::set< int, decltype(allocator2) > set22(allocator2, { 0, 2 });
    crdt::set< std::string, decltype(allocator2) > setstr2(allocator2, { 0, 3 });
    crdt::map< int, crdt::value_mv< int, decltype(allocator2) >, decltype(allocator2) > map2(allocator2, { 0, 4 });
    //crdt::map< int, crdt::value_mv< int, decltype(allocator2) >, decltype(allocator2) > map2(allocator2, { 0, 4 });

    visitor v(replica2);;
    replica1.visit(v);
    replica1.clear();

    // BOOST_TEST(set2.find(1) )
    // set2 now has the same items as set1.
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