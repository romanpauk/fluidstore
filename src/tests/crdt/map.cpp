#include <fluidstore/crdt/map.h>
#include <fluidstore/crdt/replica.h>
#include <fluidstore/crdt/value_mv.h>
#include <fluidstore/crdt/allocator.h>
#include <fluidstore/crdt/hooks/hook_extract.h>

#include <boost/test/unit_test.hpp>

static crdt::replica<> replica(0);
static crdt::allocator<> allocator(replica);

BOOST_AUTO_TEST_CASE(map_rebind)
{
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local > map(allocator);
    decltype(map)::rebind_t< decltype(allocator), crdt::tag_delta, crdt::detail::metadata_tag_btree_local, crdt::hook_default > deltamap(allocator);
}

BOOST_AUTO_TEST_CASE(map_move)
{
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local > map(allocator);
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local > map2(std::move(map));

    map = std::move(map2);
}

//typedef boost::mpl::list< std::tuple< int, int > > test_types;
BOOST_AUTO_TEST_CASE(map_basic_operations)
{
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local > map(allocator);

    auto key0 = boost::lexical_cast<int>(0);
    auto key1 = boost::lexical_cast<int>(1);
    auto value1 = crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local > (allocator);
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
    for (auto&& [k, v] : map)
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

BOOST_AUTO_TEST_CASE(map_value_mv_merge)
{
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map1(allocator);
    crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map2(allocator);
    
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

BOOST_AUTO_TEST_CASE(map_map_value_mv_merge)
{
    crdt::replica<> replica0(0);
    crdt::allocator<> allocator0(replica0);
    crdt::replica<> replica1(1);
    crdt::allocator<> allocator1(replica1);

    crdt::map< int, crdt::map< int, crdt::value_mv< int > >, decltype(allocator0), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map1(allocator0);
    crdt::map< int, crdt::map< int, crdt::value_mv< int > >, decltype(allocator1), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map2(allocator1);

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

    map1[3][1].set(1000);
    map2.erase(3);
    map1.merge(map2.extract_delta());
    BOOST_TEST((map1.at(3).at(1) == 1000));
    map2.merge(map1.extract_delta());
    BOOST_TEST((map2.at(3).at(1) == 1000));   

    // Modify map1 through iterator so there is no 'false' addition from operator [], but proper parent update through update() call.
    map1[4][40].set(400);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.at(4).at(40).get_one() == 400);
    map2.erase(4);
    BOOST_TEST((map2.find(4) == map2.end()));
    (*(*map1.find(4)).second.find(40)).second.set(4000);
    map2.merge(map1.extract_delta());
    BOOST_TEST(map2.at(4).at(40).get_one() == 4000);
}

BOOST_AUTO_TEST_CASE(map_set_merge)
{
    // TODO: the sequence is per-crdt instance, so the counters are growing wihtout holes.
    // This means that replica is global and allocator ties together sequence and replica.
    crdt::replica<> replica0(0);
    crdt::allocator<> allocator0(replica0);
    crdt::replica<> replica1(1);
    crdt::allocator<> allocator1(replica1);

    crdt::map< int, crdt::set< int >, decltype(allocator0), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map1(allocator0);
    crdt::map< int, crdt::set< int >, decltype(allocator1), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > map2(allocator1);
        
    map1[1].insert(1);
    map2.merge(map1.extract_delta());
    BOOST_TEST(*map2[1].begin() == 1);

    map2[1].insert(2);
    map1.merge(map2.extract_delta());
    BOOST_TEST(*++map1[1].begin() == 2);

    map2[1].insert(3);
    BOOST_TEST(map2[1].size() == 3);
    auto delta2 = map2.extract_delta();

    map1[1].clear();
    BOOST_TEST(map1[1].empty());
    auto delta1 = map1.extract_delta();

    map2.merge(delta1);
    BOOST_TEST(map2[1].size() == 1);
    BOOST_TEST(*map2[1].begin() == 3);

    map1.merge(delta2);
    BOOST_TEST(map2[1].size() == 1);
    BOOST_TEST(*map2[1].begin() == 3);
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(map_sizeof)
{
    PRINT_SIZEOF(std::map< int, int >);
    PRINT_SIZEOF(crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local >);
    PRINT_SIZEOF(crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract >);
    PRINT_SIZEOF(crdt::map< int, crdt::value_mv< int >, decltype(allocator), crdt::tag_delta, crdt::detail::metadata_tag_btree_local >);
}
