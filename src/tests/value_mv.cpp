#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/delta_allocator.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(value_mv_basic_operations)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> allocator(replica);

    crdt::value_mv< int, decltype(allocator) > value(allocator);

    BOOST_TEST((value == int()));
    BOOST_TEST(value.get_one() == int());

    value.set(1);
    BOOST_TEST((value == 1));
    BOOST_TEST(value.get_one() == 1);

    value = 2;
    BOOST_TEST((value == 2));
    BOOST_TEST(value.get_one() == 2);

    BOOST_TEST(*value.get_all().begin() == 2);
}

BOOST_AUTO_TEST_CASE(value_mv_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    
    crdt::allocator<> allocator(replica);
    crdt::value_mv< int, decltype(allocator) > value1(allocator);
    crdt::value_mv< int, decltype(allocator) > value2(allocator);

    // A replica is needed that propagates each delta to corresponding delta instance immediatelly.
    // Basically:
    //   value_mv will have it's delta shadow.
    //    all child objects will be tracked with shadow replica (problem: we need to generate replica id, it is a pair (replica, instance)).
    //       TODO: delta allocator will have template arg Replica, with it's own per-instance replica.
    //    after each merge, replica will propagate changes to shadow
    //    some shadows might be deltas, some not.
    // Current delta_replica is something like that, but not exactly and definitely cannot be easily used for simple testing
    // (look at set.cpp).

    // value1 = 1;
    // value1.set(1);
    //value2.merge(value1.extract_delta());
    // TODO: test
}