#include <fluidstore/crdts/replica.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/map.h>
#include <fluidstore/crdts/value_mv.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(empty_replica)
{
    typedef crdt::replica< uint64_t, uint64_t, uint64_t > replica_type;

    crdt::traits::id_sequence_type sequence;
    replica_type replica(0, sequence);
    crdt::allocator< replica_type > allocator(replica);
    typedef crdt::traits_base< replica_type > traits;

    crdt::set< int, decltype(allocator) > set(allocator, { 0, 0 });
}

struct visitor;
typedef crdt::aggregating_replica< uint64_t, uint64_t, uint64_t, crdt::instance_registry< std::pair< uint64_t, uint64_t > >, visitor > replica_type;

struct visitor
{
    visitor(replica_type& r)
        : replica_(r)
    {}

    template < typename T > void visit(const T& instance)
    {
        replica_.merge(instance);
    }

    replica_type& replica_;
};

BOOST_AUTO_TEST_CASE(aggregating_replica)
{
    typedef crdt::traits_base< replica_type > traits;

    crdt::traits::id_sequence_type sequence1;
    traits::replica_type replica1(1, sequence1);
    traits::allocator_type allocator1(replica1);

    crdt::traits::id_sequence_type sequence2;
    traits::replica_type replica2(2, sequence2);
    traits::allocator_type allocator2(replica2);

    crdt::set< int, decltype(allocator1) > set1(allocator1, { 0, 1 });
    set1.insert(1);
    set1.insert(2);
    crdt::set< double, decltype(allocator1) > set11(allocator1, { 0, 2 });
    set11.insert(1.1);
    set11.insert(2.2);
    crdt::set< std::string, decltype(allocator1) > setstr1(allocator1, { 0, 3 });
    setstr1.insert("Hello");

    crdt::map< int, crdt::value_mv< int, decltype(allocator1) >, decltype(allocator1) > map1(allocator1, { 0, 4 });
    map1[1] = 1;
    map1[2] = 2;
    map1[3] = 3;

    crdt::map< int, crdt::map< int, crdt::value_mv< int, decltype(allocator1) >, decltype(allocator1) >, decltype(allocator1) > map11(allocator1, { 0, 5 });
    map11[1][10] = 100;
    map11[2][20] = 200;

    // TODO: this requires operator <.
    //crdt::map< int, crdt::value_mv< crdt::set< int, decltype(allocator1) >, decltype(allocator1) >, decltype(allocator1) > map3(allocator1, { 0, 6 });

    crdt::set< int, decltype(allocator2) > set2(allocator2, { 0, 1 });
    crdt::set< int, decltype(allocator2) > set22(allocator2, { 0, 2 });
    crdt::set< std::string, decltype(allocator2) > setstr2(allocator2, { 0, 3 });
    crdt::map< int, crdt::value_mv< int, decltype(allocator2) >, decltype(allocator2) > map2(allocator2, { 0, 4 });
    crdt::map< int, crdt::map< int, crdt::value_mv< int, decltype(allocator2) >, decltype(allocator2) >, decltype(allocator2) > map22(allocator2, { 0, 5 });

    visitor v(replica2);;
    replica1.visit(v);
    replica1.clear();
}