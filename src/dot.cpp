#include <crdt/dot.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_set_test)
{
	crdt::set< uint64_t, uint64_t, int > set;
	BOOST_ASSERT(set.find(0) == false);
	set.insert(1);
	BOOST_ASSERT(set.find(1) == true);
	set.erase(1);
	BOOST_ASSERT(set.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_test)
{
	crdt::map< uint64_t, uint64_t, int, crdt::value< int > > map;
	BOOST_ASSERT(map.find(0) == false);
	map.insert(1, 1);
	BOOST_ASSERT(map.find(1) == true);
	map.erase(1);
	BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
	crdt::map< uint64_t, uint64_t, int, crdt::set< uint64_t, uint64_t, int > > map;
	BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, );
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
	crdt::map< uint64_t, uint64_t, int, crdt::value_mv< uint64_t, uint64_t, int > > map;
	BOOST_ASSERT(map.find(0) == false);
	map.insert(1, 1);
	BOOST_ASSERT(map.find(1) == true);
	map.erase(1);
	BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
	crdt::map< uint64_t, uint64_t, int, crdt::map< uint64_t, uint64_t, int, crdt::value_mv< uint64_t, uint64_t, int > > > map;
	//BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, 1);
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}