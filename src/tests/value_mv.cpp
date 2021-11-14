#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/hook_extract.h>
#include <fluidstore/crdts/allocator.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(value_mv_basic_operations)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> allocator(replica);

    crdt::value_mv< int, decltype(allocator), crdt::tag_state > value(allocator);

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

    crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > value1(allocator);
    crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > value2(allocator);

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

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(value_mv_sizeof)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);
    crdt::allocator<> allocator(replica);

    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_state >);
    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract >);
    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_delta >);
}
