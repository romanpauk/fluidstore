#include <crdt/dot.h>

#include <boost/test/unit_test.hpp>

typedef crdt::allocator< uint64_t, void > allocator;

BOOST_AUTO_TEST_CASE(dot_allocator_test)
{
	struct X
	{
		typedef crdt::allocator< uint64_t, void > allocator_type;

		X(allocator_type)
		{}
	};

	crdt::allocator< uint64_t, void > alloc(1);

	std::map< int, X, std::less< int >,
		std::scoped_allocator_adaptor< crdt::allocator< uint64_t, void > >
	> v(alloc);

	X& x = v[1];
}

BOOST_AUTO_TEST_CASE(dot_set_test)
{
	allocator alloc(0);
	crdt::set< allocator, uint64_t, uint64_t, int > set(alloc);
	BOOST_ASSERT(set.find(0) == false);
	set.insert(1);
	BOOST_ASSERT(set.find(1) == true);
	set.erase(1);
	BOOST_ASSERT(set.find(1) == false);
}


BOOST_AUTO_TEST_CASE(dot_map_test)
{
	allocator alloc(0);
	crdt::map< allocator, uint64_t, uint64_t, int, crdt::value< allocator, int > > map(alloc);
	BOOST_ASSERT(map.find(0) == false);
	map.insert(1, 1);
	BOOST_ASSERT(map.find(1) == true);
	map.erase(1);
	BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
	allocator alloc(0);
	crdt::map< allocator, uint64_t, uint64_t, int, crdt::set< allocator, uint64_t, uint64_t, int > > map(alloc);
	BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, );
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
	allocator alloc(0);
	crdt::map< allocator, uint64_t, uint64_t, int, crdt::value_mv< allocator, uint64_t, uint64_t, int > > map(alloc);
	BOOST_ASSERT(map.find(0) == false);
	// map.insert(1, 1);
	BOOST_ASSERT(map.find(1) == true);
	map.erase(1);
	BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
	allocator alloc(0);
	crdt::map< allocator, uint64_t, uint64_t,
		int, crdt::map< allocator, uint64_t, uint64_t, int, crdt::value_mv< allocator, uint64_t, uint64_t, int > > > map(alloc);
	//BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, 1);
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}