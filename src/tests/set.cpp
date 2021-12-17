#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/hook_extract.h>

#include <boost/test/unit_test.hpp>

static crdt::replica<> replica(0);
static crdt::allocator<> allocator(replica);

BOOST_AUTO_TEST_CASE(set_rebind)
{
    crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > set(allocator);
    decltype(set)::rebind_t< decltype(allocator), crdt::tag_delta, crdt::hook_default > deltaset(allocator);
}

BOOST_AUTO_TEST_CASE(set_move)
{
    crdt::set< int, decltype(allocator), crdt::tag_state > set(allocator);
    crdt::set< int, decltype(allocator), crdt::tag_state > set2(std::move(set));
    
    set = std::move(set2);
}

typedef boost::mpl::list< int, double > test_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(set_basic_operations, T, test_types)
{
    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::hook_extract > set(allocator);

    auto value0 = boost::lexical_cast<T>(0);
    auto value1 = boost::lexical_cast<T>(1);

    // Empty set
    BOOST_TEST((set.find(value0) == set.end()));
    BOOST_TEST((set.begin() == set.end()));
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());
    BOOST_TEST(set.erase(value0) == 0);

    // Insert element
    auto pb = set.insert(value1);
    BOOST_TEST(pb.second == true);
    BOOST_TEST((pb.first == set.find(value1)));
    BOOST_TEST((set.find(value1) != set.end()));
    BOOST_TEST((*set.find(value1) == value1));
    BOOST_TEST((++set.begin() == set.end()));
    BOOST_TEST((set.begin() == --set.end()));
    BOOST_TEST(set.size() == 1);
    BOOST_TEST(!set.empty());

    // Insert second time, erase by iterator
    pb = set.insert(value1);
    BOOST_TEST(pb.second == false);
    BOOST_TEST(set.size() == 1);
    BOOST_TEST((set.erase(pb.first) == set.end()));
    BOOST_TEST((set.find(value1) == set.end()));
    BOOST_TEST(set.empty());

    // Insert, erase by key
    pb = set.insert(value1);
    BOOST_TEST(pb.second == true);
    BOOST_TEST(set.erase(value1) == 1);
    BOOST_TEST(set.empty());

    // Iterate
    set.insert(value1);
    size_t iters = 0;
    for (auto& v : set)
    {
        ++iters;
        BOOST_TEST(v == value1);
    }
    BOOST_TEST(iters == 1);

    set.clear();
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());
}

BOOST_AUTO_TEST_CASE(set_extract)
{
    crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > set(allocator);
    set.insert(1);
    set.insert(2);
    BOOST_TEST(set.extract_delta().size() == 2);
    BOOST_TEST(set.extract_delta().size() == 0);
}

BOOST_AUTO_TEST_CASE(set_merge)
{
    crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > set1(allocator);
    crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > set2(allocator);

    set1.insert(1);
    set2.merge(set1.extract_delta());
    BOOST_TEST(set2.size() == 1);
    BOOST_TEST((set2.find(1) != set2.end()));
    
    set2.erase(1);    
    set1.merge(set2.extract_delta());
    BOOST_TEST(set1.empty());
    BOOST_TEST((set1.find(1) == set1.end()));

    set2.insert(22);
    set2.insert(222);
    set1.merge(set2.extract_delta());
    BOOST_TEST(set1.size() == 2);
    BOOST_TEST((set1.find(22) != set1.end()));
    BOOST_TEST((set1.find(222) != set1.end()));

    set2.clear();
    BOOST_TEST(set2.empty());
    set1.insert(11);
    set1.merge(set2.extract_delta());
    BOOST_TEST(set1.size() == 1);
    BOOST_TEST((set1.find(11) != set1.end()));
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(set_sizeof)
{
    PRINT_SIZEOF(std::set< int >);
    PRINT_SIZEOF(crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_default >);
    PRINT_SIZEOF(crdt::set< int, decltype(allocator), crdt::tag_state, crdt::hook_extract >);
    PRINT_SIZEOF(crdt::set< int, decltype(allocator), crdt::tag_delta >);
}