#include <iostream>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <numeric>

namespace std
{
    std::string to_string(const char* s) { return s; }
    std::string to_string(const std::string& s) { return s; }
    std::string to_string(std::string&& s) { return std::move(s); }
}

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

void set_test();
void map_test();
void set_g_test();
void set_or_test();
void value_mv_test();
void counter_g_test();
void counter_pn_test();

int main()
{
    sqlstl::db db(":memory:");
    sqlstl::allocator allocator(db);

    set_test();
    map_test();

    counter_g_test();
    counter_pn_test();
    set_g_test();
    set_or_test();
    value_mv_test();

    // Sql-based map of delta-counters that themselves are sql-based.
    sqlstl::map< 
        int, 
        crdt::counter_pn< int, int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator
    > mapcounterg(allocator, "MapCounterG");

    mapcounterg[1].add(1, 4);
    mapcounterg[1].add(2, 4);
    assert(mapcounterg[0].value() == 0);
    assert(mapcounterg[1].value() == 8);

    sqlstl::map<
        int,
        crdt::set_g< int,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::allocator
    > mapsetg(allocator, "MapSetG");

    mapsetg[1].insert(1);
    mapsetg[1].insert(2);
    assert(mapsetg[0].size() == 0);
    assert(mapsetg[1].size() == 2);

    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg1(allocator, "CrdtMapG1");
    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg2(allocator, "CrdtMapG2");

    mapg1[1].insert(1);
    mapg1[1].insert(2);
    mapg2[1].insert(3);
    mapg2[2].insert(1);
    mapg1.merge(mapg2);
    assert(mapg1[1].size() == 3);
} 
