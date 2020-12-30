#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/allocator.h>

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

    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator), crdt::extract_delta_hook > map1(allocator);
    crdt::map< int, crdt::value_mv< int, decltype(allocator) >, decltype(allocator), crdt::extract_delta_hook > map2(allocator);

    auto& value = map1[1];
    value.set(1);
    
    // The delta does not contain change from map element.
    // One way is to send the change to parent and let parent distribute it properly to it's delta.
    // To send the change to parent we will need to establish parent/child relation-ship somehow, using allocators/replicas.
    // Other may be to somehow map the delta to parent's delta.

    auto delta = map1.extract_delta();
}
