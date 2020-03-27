#define NOMINMAX

#include <iostream>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <numeric>

#define BOOST_TEST_MODULE fluidstore
#include <boost/test/unit_test.hpp>

#include <sqlstl/allocator.h>
#include <sqlstl/map.h>
#include <sqlstl/set.h>
#include <sqlstl/tuple.h>

#include <crdt/counter_g.h>
#include <crdt/counter_pn.h>
#include <crdt/set_g.h>
#include <crdt/set_or.h>
#include <crdt/map_g.h>
#include <crdt/value_lww.h>
#include <crdt/value_mv.h>
#include <crdt/traits.h>

// #include <schema/schema.h>

// void schema_test();

BOOST_AUTO_TEST_CASE(test_case_name)
//int main()
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);

    // schema_test();

    sqlstl::allocator<void> allocator(factory);

    // Sql-based map of delta-counters that themselves are sql-based.
    sqlstl::map< 
        int, 
        crdt::counter_pn< int, int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator<void>
    > mapcounterg(allocator);

    mapcounterg[1].add(1, 4);
    mapcounterg[1].add(2, 4);
    BOOST_ASSERT(mapcounterg[0].value() == 0);
    BOOST_ASSERT(mapcounterg[1].value() == 8);

    sqlstl::map<
        int,
        crdt::set_g< int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator<void>
    > mapsetg(allocator);

    mapsetg[1].insert(1);
    mapsetg[1].insert(2);
    BOOST_ASSERT(mapsetg[0].size() == 0);
    BOOST_ASSERT(mapsetg[1].size() == 2);

    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg1(allocator);
    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg2(allocator);

    //mapg1[1].insert(1);
    //mapg1[1].insert(2);
    //mapg2[1].insert(3);
    //mapg2[2].insert(1);
    //mapg1.merge(mapg2);
    //BOOST_ASSERT(mapg1[1].size() == 3);
} 
