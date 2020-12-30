#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>

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

    value.set(2); // TODO: operator = 
    BOOST_TEST((value == 2));
    BOOST_TEST(value.get_one() == 2);

    BOOST_TEST(*value.get_all().begin() == 2);
}

BOOST_AUTO_TEST_CASE(value_mv_merge)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(1, sequence);
    crdt::allocator<> allocator(replica);

    crdt::value_mv< int, decltype(allocator), crdt::extract_delta_hook > value1(allocator);
    crdt::value_mv< int, decltype(allocator), crdt::extract_delta_hook > value2(allocator);

    value1.set(1);
    value2.merge(value1.extract_delta());
    BOOST_TEST(value2.get_one() == 1);

    value1.set(2);
    value2.merge(value1.extract_delta());
    BOOST_TEST(value2.get_one() == 2);

    value2.set(22);
    value1.set(0);
    value2.merge(value1.extract_delta());
    BOOST_TEST(value2.get_all().size() == 2);
}