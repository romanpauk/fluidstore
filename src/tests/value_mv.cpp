#include <fluidstore/crdt/value_mv.h>
#include <fluidstore/crdt/hooks/hook_extract.h>
#include <fluidstore/crdt/allocator.h>

#include <boost/test/unit_test.hpp>

static crdt::replica<> replica(0);
static crdt::allocator<> allocator(replica);

BOOST_AUTO_TEST_CASE(value_mv_rebind)
{   
    crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > value(allocator);
    decltype(value)::rebind_t< decltype(allocator), crdt::tag_delta, crdt::hook_default > deltavalue(allocator);
}

BOOST_AUTO_TEST_CASE(value_mv_basic_operations)
{
    crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract > value(allocator);

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

BOOST_AUTO_TEST_CASE(value_mv_set)
{
    crdt::replica<> replica0(0);
    crdt::allocator<> allocator0(replica0);
    crdt::replica<> replica1(1);
    crdt::allocator<> allocator1(replica1);

    crdt::value_mv< crdt::set< int >, decltype(allocator0), crdt::tag_state, crdt::hook_extract > value0(allocator0);    
    crdt::value_mv< crdt::set< int >, decltype(allocator1), crdt::tag_state, crdt::hook_extract > value1(allocator1);
}

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(value_mv_sizeof)
{
    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_default >);
    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_state, crdt::hook_extract >);
    PRINT_SIZEOF(crdt::value_mv< int, decltype(allocator), crdt::tag_delta >);
}
