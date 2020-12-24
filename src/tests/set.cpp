#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/replica.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(set_basic_operations)
{
    crdt::traits::id_sequence_type sequence;
    crdt::traits::replica_type replica(0, sequence);
    crdt::traits::allocator_type alloc(replica);
    crdt::set< int, decltype(alloc) > set(alloc, { 0, 0 });
    BOOST_TEST((set.find(0) == set.end()));
    BOOST_TEST((set.begin() == set.end()));
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());

    auto pb = set.insert(1);
    BOOST_TEST(pb.second == true);
    BOOST_TEST((pb.first == set.find(1)));
    BOOST_TEST((set.find(1) != set.end()));
    BOOST_TEST((*set.find(1) == 1));
    BOOST_TEST((++set.begin() == set.end()));
    BOOST_TEST((set.begin() == --set.end()));
    pb = set.insert(1);
    BOOST_TEST(pb.second == false);
    BOOST_TEST(set.size() == 1);
    BOOST_TEST(!set.empty());

    set.erase(1);
    BOOST_TEST((set.find(1) == set.end()));
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());

    set.insert(1);
    BOOST_TEST(!set.empty());
    size_t iters = 0;
    for (auto& v : set)
    {
        ++iters;
        BOOST_TEST(v == 1);
    }
    BOOST_TEST(iters == 1);
    set.clear();
    BOOST_TEST(set.size() == 0);
    BOOST_TEST(set.empty());
}

BOOST_AUTO_TEST_CASE(set_merge)
{
    crdt::traits::id_sequence_type sequence1;
    crdt::traits::replica_type replica1(1, sequence1);
    crdt::traits::allocator_type alloc1(replica1);
    crdt::set< int, decltype(alloc1) > set1(alloc1, { 1, 1 });

    crdt::traits::id_sequence_type sequence2;
    crdt::traits::replica_type replica2(2, sequence2);
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