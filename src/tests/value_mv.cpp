#include <fluidstore/crdts/value_mv.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/tagged_allocator.h>
#include <fluidstore/crdts/delta_hook.h>

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

    crdt::value_mv< int, decltype(allocator), crdt::delta_hook > value1(allocator);
    crdt::value_mv< int, decltype(allocator), crdt::delta_hook > value2(allocator);

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

BOOST_AUTO_TEST_CASE(value_mv_tagged_allocator_delta)
{
    crdt::id_sequence<> sequence;
    crdt::replica<> replica(0, sequence);

    crdt::arena< 32768 > arena;
    crdt::arena_allocator< void, crdt::allocator<> > deltaallocator(arena, replica);
    crdt::allocator<> stateallocator(replica);

    crdt::tagged_allocator< crdt::replica<>, int, decltype(stateallocator), decltype(deltaallocator) > allocator(replica, stateallocator, deltaallocator);

    crdt::value_mv< int, decltype(allocator), crdt::delta_hook > value(allocator);

    {
        value.set(1);
        BOOST_TEST(arena.get_allocated() > 0);
        value.extract_delta();
    }

    BOOST_TEST(arena.get_allocated() == 0);
}