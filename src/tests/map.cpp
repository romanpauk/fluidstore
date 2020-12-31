#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/delta_hook.h>

#include <boost/test/unit_test.hpp>

//typedef boost::mpl::list< std::tuple< int, int > > test_types;
BOOST_AUTO_TEST_CASE(map_basic_operations)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> allocator(replica);
    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator) > map(allocator);

    auto key0 = boost::lexical_cast<int>(0);
    auto key1 = boost::lexical_cast<int>(1);
    auto value1 = crdt::value_mv< int, decltype(allocator) >(allocator);
    value1.set(1);

    // Empty map
    BOOST_TEST((map.find(key0) == map.end()));
    BOOST_TEST((map.begin() == map.end()));
    BOOST_TEST(map.size() == 0);
    BOOST_TEST(map.empty());
    BOOST_TEST(map.erase(key0) == 0);

    // Insert element
    auto pb = map.insert(key1, value1);
    BOOST_TEST(pb.second == true);
    BOOST_TEST((pb.first == map.find(key1)));
    BOOST_TEST(((*pb.first).first == key1));
    BOOST_TEST(((*pb.first).second == value1.get_one()));
    BOOST_TEST((map.find(key1) != map.end()));
    BOOST_TEST((++map.begin() == map.end()));
    BOOST_TEST((map.begin() == --map.end()));
    BOOST_TEST(map.size() == 1);
    BOOST_TEST(!map.empty());

    // Insert second time, erase by iterator
    pb = map.insert(key1, value1);
    BOOST_TEST(pb.second == false);
    BOOST_TEST(map.size() == 1);
    BOOST_TEST((map.erase(pb.first) == map.end()));
    BOOST_TEST((map.find(key1) == map.end()));
    BOOST_TEST(map.empty());

    // Insert, erase by key
    pb = map.insert(key1, value1);
    BOOST_TEST(pb.second == true);
    BOOST_TEST(map.erase(key1) == 1);
    BOOST_TEST(map.empty());

    // Iterate
    map.insert(key1, value1);
    size_t iters = 0;
    for (auto& [k, v] : map)
    {
        ++iters;
        BOOST_TEST(k == key1);
        BOOST_TEST((v == value1.get_one()));
    }
    BOOST_TEST(iters == 1);
    BOOST_TEST((map.at(key1) == value1.get_one()));

    map.clear();
    BOOST_TEST(map.size() == 0);
    BOOST_TEST(map.empty());
}

BOOST_AUTO_TEST_CASE(map_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::allocator<> allocator(replica);

    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator), crdt::delta_hook > map1(allocator);
    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator), crdt::delta_hook > map2(allocator);

    map1[1].set(1);
    map2.merge(map1.extract_delta());
    BOOST_TEST((map2.at(1) == 1));
    
    map1[1].set(2);
    map2[1].set(22);
    map2.merge(map1.extract_delta());
    BOOST_TEST((map2.at(1).get_all().size() == 2));

    BOOST_TEST(map1.erase(1) == 1);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.size() == 0);
    
    map1[1].set(10);
    map2.merge(map1.extract_delta());
    map1.erase(1);
    map2[1].set(11);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.size() == 1);
    map1.merge(map2.extract_delta());
    map1.erase(1);
    BOOST_TEST(map1.size() == 0);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.size() == 0);
}

BOOST_AUTO_TEST_CASE(map_map_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::allocator<> allocator(replica);

    crdt::map< int, crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator) >, decltype(allocator), crdt::delta_hook > map1(allocator);
    crdt::map< int, crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator) >, decltype(allocator), crdt::delta_hook > map2(allocator);

    map1[1][10].set(1);
    map2.merge(map1.extract_delta());
    BOOST_TEST((map2.at(1).at(10) == 1));

    map1[2][10].set(10);
    map2[2][10].set(100);
    map2.merge(map1.extract_delta());
    BOOST_TEST((map2.at(2).at(10).get_all().size() == 2));

    map1[3][1].set(10);
    map1[3][10].set(100);
    map2[3][2].set(20);
    map2[3][20].set(200);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.at(3).size() == 4);
    map1.merge(map2.extract_delta());
    BOOST_TEST(map1.at(3).size() == 4);
    
    map1[3].erase(1);
    BOOST_TEST(map1.at(3).size() == 3);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.at(3).size() == 3);

    map1[3].erase(10);
    BOOST_TEST(map1.at(3).size() == 2);
    map2[3][10].set(1000);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.at(3).size() == 3);
}

BOOST_AUTO_TEST_CASE(map_tagged_allocator_delta)
{
    /*
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);

    crdt::arena< 8192 > arena;
    crdt::tagged_type< crdt::tag_state, crdt::allocator<> > a1(replica);
    crdt::tagged_type< crdt::tag_delta, crdt::arena_allocator< void, crdt::allocator<> > > a2(arena, replica);
    crdt::tagged_allocator< crdt::replica<>, decltype(a1), decltype(a2) > allocator(replica, a1, a2);

    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator), crdt::delta_hook > map(allocator);
    {
        map[1].set(1);
        BOOST_TEST(arena.get_allocated() > 0);
        map.extract_delta();
    }

    BOOST_TEST(arena.get_allocated() == 0);
    */
}