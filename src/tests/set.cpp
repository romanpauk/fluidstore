#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/delta_hook.h>
#include <fluidstore/crdts/delta_replica.h>
#include <fluidstore/crdts/registry.h>

#include <boost/test/unit_test.hpp>

typedef boost::mpl::list< int, double > test_types;
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
    crdt::registry<> registry;
    crdt::id_sequence<> sequence;
    crdt::delta_replica< decltype(registry) > replica(1, sequence, registry);
    crdt::allocator< decltype(replica) > allocator(replica);

    crdt::set< int, decltype(allocator), crdt::registry_hook< decltype(registry), crdt::allocator<> > > set1(allocator);
    set1.insert(1);

    // Registered classes can be accesed through registry. Not sure about the delta accessing interface.
    //registry.get_deltas([&](const auto& delta) {});
    //registry.clear_deltas();

    // TODO: unfinished
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(set_sizeof)
{
    PRINT_SIZEOF(std::set< int >);

    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);

    {
        crdt::allocator<> allocator(replica);
        PRINT_SIZEOF(crdt::set< int, decltype(allocator) >);
        PRINT_SIZEOF(crdt::set_base< int, decltype(allocator), crdt::tag_delta, crdt::default_delta_hook, void >);
        PRINT_SIZEOF(crdt::set_base< int, decltype(allocator), crdt::tag_state, crdt::default_state_hook, 
            crdt::set_base< int, decltype(allocator), crdt::tag_delta, crdt::default_delta_hook, void > >);
    }
}