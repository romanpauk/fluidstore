#include <iostream>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <numeric>

#include <sqlstl/factory.h>
#include <sqlstl/map.h>
#include <sqlstl/set.h>
#include <crdt/counter_g.h>
#include <crdt/counter_pn.h>
#include <crdt/set_g.h>
#include <crdt/set_or.h>
#include <crdt/map_g.h>
#include <crdt/traits.h>

void set_test();
void map_test();

int main()
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);

    set_test();
    map_test();

    // Fully in-memory counter
    crdt::delta_counter_g< int, int, 
        crdt::traits< crdt::memory > > counterg_memory;

    // Sqlite-backed counter with in memory delta, eg. counterg_sqlite.add(1, 1) will return in-memory counter_g<>, that 
    // can be sent to replicas and merged with their counter_g.
    crdt::delta_counter_g< int, int, 
        crdt::traits< crdt::memory >, 
        crdt::traits< crdt::sqlite >
    > counterg(factory, "CounterG");

    counterg.add(1, 1);
    counterg.add(2, 3);
    assert(counterg.value() == 4);

    {
        // Due to sqlite persistence, we will reconnect to existing instance.
        // There might be cases when we would like not to reconnect.
        decltype(counterg) counterg1(factory, "CounterG");
        assert(counterg1.value() == 4);
    }

    // Positive-negative counter
    crdt::delta_counter_pn< int, int,
        crdt::traits< crdt::memory >,
        crdt::traits< crdt::sqlite >
    > counterpn(factory, "CounterPN");

    counterpn.add(1, 1);
    counterpn.add(2, 1);
    counterpn.sub(3, 3);

    assert(counterpn.value() == -1);

    crdt::delta_set_g< int,
        crdt::traits< crdt::memory >,
        crdt::traits< crdt::sqlite >,
        sqlstl::factory
    > setg(factory, "SetG");

    assert(setg.size() == 0);
    setg.insert(1);
    assert(setg.size() == 1);
    setg.insert(2);
    assert(setg.size() == 2);

// Delta version has some issues with removal
/*
    crdt::delta_set_or< int,
        crdt::traits< crdt::memory >,
        crdt::traits< crdt::sqlite >
    > setorx(factory, "SetOR2");
    setorx.erase(1);
*/
    crdt::set_or< int,
        crdt::traits< crdt::sqlite >
    //> setor;
    > setor(factory, "SetOR2");

    setor.insert(1);
    assert(setor.size() == 1);
    setor.insert(1);
    assert(setor.size() == 1);
    assert(setor.find(1) == true);
    // Delta version has some issues.
    setor.erase(1);
    assert(setor.size() == 0);
    assert(setor.find(1) == false);

    // Sql-based map of delta-counters that themselves are sql-based.
    sqlstl::map< 
        int, 
        crdt::delta_counter_pn< int, int,
            crdt::traits< crdt::memory >,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::factory
    > mapcounterg(factory, "MapCounterG");

    mapcounterg[1].add(1, 4);
    mapcounterg[1].add(2, 4);
    assert(mapcounterg[0].value() == 0);
    assert(mapcounterg[1].value() == 8);

    sqlstl::map<
        int,
        crdt::delta_set_g< int,
            crdt::traits< crdt::memory >,
            crdt::traits< crdt::sqlite >
        >,
        sqlstl::factory
    > mapsetg(factory, "MapSetG");

    mapsetg[1].insert(1);
    mapsetg[1].insert(2);
    assert(mapsetg[0].size() == 0);
    assert(mapsetg[1].size() == 2);

    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg1(factory, "CrdtMapG1");
    crdt::map_g< int, crdt::set_g< int, crdt::traits< crdt::sqlite > >, crdt::traits< crdt::sqlite > > mapg2(factory, "CrdtMapG2");

    mapg1[1].insert(1);
    mapg1[1].insert(2);
    mapg2[1].insert(3);
    mapg2[2].insert(1);
    mapg1.merge(mapg2);
    assert(mapg1[1].size() == 3);

} 
