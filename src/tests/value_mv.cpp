#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/delta_allocator.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(value_mv_basic_operations)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> alloc(replica);

    crdt::value_mv< int, decltype(alloc) > value(alloc);

    BOOST_TEST((value == int()));
    BOOST_TEST(value.get() == int());

    value.set(1);
    BOOST_TEST((value == 1));
    BOOST_TEST(value.get() == 1);

    value = 2;
    BOOST_TEST((value == 2));
    BOOST_TEST(value.get() == 2);

    crdt::value_mv< int, decltype(alloc) > value2(alloc);
    // value2 = value;
    // BOOST_TEST((value2 == 2));
}

BOOST_AUTO_TEST_CASE(value_mv_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::delta_allocator< crdt::value_mv< int, crdt::allocator<>, crdt::tag_delta > > allocator(replica);

    crdt::value_mv< int, decltype(allocator) > v1(allocator, { 0, 1 });
    crdt::value_mv< int, decltype(allocator) > v2(allocator, { 0, 2 });

    // TODO: test
}
