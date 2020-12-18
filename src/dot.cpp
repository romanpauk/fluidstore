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
	crdt::set< int, allocator, uint64_t, uint64_t > set(alloc);
	BOOST_ASSERT(set.find(0) == set.end());
	set.insert(1);
	BOOST_ASSERT(set.find(1) != set.end());
	set.erase(1);
	BOOST_ASSERT(set.find(1) == set.end());
}


BOOST_AUTO_TEST_CASE(dot_map_test)
{
	allocator alloc(0);
	crdt::map< int, crdt::value< int, allocator >, allocator, uint64_t, uint64_t > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	map.insert(1, crdt::value< int, allocator >(alloc, 1));
	BOOST_ASSERT(map.find(1) != map.end());
	map.erase(1);
	BOOST_ASSERT(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
	allocator alloc(0);
	crdt::map< int, crdt::set< int, allocator, uint64_t, uint64_t >, allocator, uint64_t, uint64_t > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	//map.insert(1, );
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
	allocator alloc(0);
	crdt::map< int, crdt::value_mv< int, allocator, uint64_t, uint64_t >, allocator, uint64_t, uint64_t > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	// map.insert(1, 1);
	BOOST_ASSERT(map.find(1) != map.end());
	map.erase(1);
	BOOST_ASSERT(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
	allocator alloc(0);
	crdt::map< 
		int, 
		crdt::map< 
		    int, 
		    crdt::value_mv< int, allocator, uint64_t, uint64_t >, allocator, uint64_t, uint64_t 
		>, 
		allocator, uint64_t, uint64_t
	> map(alloc);

	//BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, 1);
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}
