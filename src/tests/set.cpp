#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/delta_hook.h>
#include <fluidstore/crdts/delta_replica.h>
#include <fluidstore/crdts/tagged_collection.h>

#include <boost/test/unit_test.hpp>

typedef boost::mpl::list<int, double, std::string > test_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(set_basic_operations, T, test_types)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> allocator(replica);
    crdt::set< T, decltype(allocator) > set(allocator);

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

BOOST_AUTO_TEST_CASE(set_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::allocator<> allocator(replica);
    
    crdt::set< int, decltype(allocator), crdt::delta_hook > set1(allocator);
    crdt::set< int, decltype(allocator), crdt::delta_hook > set2(allocator);

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

BOOST_AUTO_TEST_CASE(set_merge_replica)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::allocator<> replica_allocator(replica);    
    crdt::delta_replica< crdt::system<>, crdt::allocator<> > delta_replica(replica_allocator);
    crdt::allocator< decltype(delta_replica) > delta_allocator(delta_replica);

    crdt::set< int, decltype(delta_allocator), crdt::delta_replica_hook > set1(delta_allocator);
    set1.insert(1);
}

BOOST_AUTO_TEST_CASE(set_tagged_allocator)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);

    crdt::arena< 8192 > arena;
    crdt::tagged_type< crdt::tag_state, crdt::allocator<> > a1(replica);
    crdt::tagged_type< crdt::tag_delta, crdt::arena_allocator< void, crdt::allocator<> > > a2(arena, replica);
    crdt::tagged_allocator< crdt::replica<>, decltype(a1), decltype(a2) > allocator(replica, a1, a2);

    {
        allocator.get< crdt::tag_state >();
        allocator.get< crdt::tag_delta >();
        decltype(allocator)::type< crdt::tag_delta > a(arena, replica);
        crdt::allocator_traits< decltype(a1) >::get_allocator< crdt::tag_state >(a1);
        crdt::allocator_traits< decltype(allocator) >::get_allocator< crdt::tag_delta >(allocator);
        crdt::allocator_traits< decltype(allocator) >::allocator_type< crdt::tag_delta > d(allocator);
    }

    crdt::set< int, decltype(allocator) > set(allocator);
    set.insert(1);
    BOOST_TEST(arena.get_allocated() == 0);
}

BOOST_AUTO_TEST_CASE(set_tagged_allocator_delta)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);

    crdt::arena< 8192 > arena;
    crdt::tagged_type< crdt::tag_state, crdt::allocator<> > a1(replica);
    crdt::tagged_type< crdt::tag_delta, crdt::arena_allocator< void, crdt::allocator<> > > a2(arena, replica);
    crdt::tagged_allocator< crdt::replica<>, decltype(a1), decltype(a2) > allocator(replica, a1, a2);

    crdt::set< int, decltype(allocator), crdt::delta_hook > set(allocator);
    
    {
        set.insert(1);
        BOOST_TEST(arena.get_allocated() > 0);
        set.extract_delta();
    }

    BOOST_TEST(arena.get_allocated() == 0);
}