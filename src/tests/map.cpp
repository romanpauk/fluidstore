#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/value.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(map_basic_operations)
{
    crdt::traits::id_sequence_type sequence;
    crdt::traits::replica_type replica(0, sequence);
    crdt::traits::allocator_type alloc(replica);
    crdt::map< int, crdt::value< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    BOOST_TEST((map.find(0) == map.end()));

    //map.insert(1, crdt::value< int, crdt::traits >(alloc, 11));
    //BOOST_TEST((map.find(1) != map.end()));
    //BOOST_TEST(((*map.find(1)).first == 1));
    //BOOST_TEST(((*map.find(1)).second == 11));
    //map.erase(1);
    //BOOST_TEST((map.find(1) == map.end()));
}

BOOST_AUTO_TEST_CASE(map_merge)
{
    crdt::traits::id_sequence_type sequence;
    crdt::traits::replica_type replica(0, sequence);
    crdt::traits::allocator_type alloc(replica);
    //crdt::map< int, crdt::set< int, decltype(alloc) >, decltype(alloc) > map(alloc, { 0, 1 });
    //BOOST_TEST((map.find(0) == map.end()));
    //map.insert(1, );
    //BOOST_TEST(map.find(1) == true);
    //map.erase(1);
    //BOOST_TEST(map.find(1) == false);
}
