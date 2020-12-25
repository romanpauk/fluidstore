#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/delta_allocator.h>
#include <fluidstore/crdts/allocator.h>

#include <boost/test/unit_test.hpp>

typedef boost::mpl::list<int, double, std::string > test_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(set_basic_operations, T, test_types)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> alloc(replica);
    crdt::set< T, decltype(alloc) > set(alloc, { 0, 0 });

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

BOOST_AUTO_TEST_CASE(set_delta)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica1(1, sequence);
    crdt::delta_allocator< crdt::set< int, crdt::allocator<> > > allocator1(replica1);

    crdt::replica<> replica2(2, sequence);
    crdt::delta_allocator< crdt::set< int, crdt::allocator<> > > allocator2(replica2);

    crdt::set< int, decltype(allocator1) > set1(allocator1, { 1, 1 });
    crdt::set< int, decltype(allocator2) > set2(allocator2, { 2, 1 });
    
    set1.insert(1);
    set1.insert(2);
    set1.insert(3);

    // Conflicting insert of 1, non-observed by set 1
    set2.insert(1);

    set2.merge(set1.extract_delta());
    set1.clear();
    set2.merge(set1.extract_delta());

    // When merging to delta, we can't collapse coun
    BOOST_TEST(set2.size() == 1);
}

BOOST_AUTO_TEST_CASE(set_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::delta_allocator< crdt::set< int, crdt::allocator<> > > allocator(replica);

    crdt::set< int, decltype(allocator) > set1(allocator, { 0, 1 });
    crdt::set< int, decltype(allocator) > set2(allocator, { 0, 2 });

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

    set2.clear();
    set1.insert(11);
    set1.merge(set2.extract_delta());
    BOOST_TEST(set1.size() == 1);
    BOOST_TEST((set1.find(11) != set1.end()));
}
