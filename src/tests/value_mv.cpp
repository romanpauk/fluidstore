#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(value_mv_basic_operations)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> alloc(replica);
    crdt::value_mv< int, decltype(alloc) > value(alloc);
    BOOST_TEST(value.get() == int());
    // value = 1;
    value.set(1);
    BOOST_TEST(value.get() == 1);
}