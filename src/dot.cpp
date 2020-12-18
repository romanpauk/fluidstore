#include <crdt/dot.h>

#include <boost/test/unit_test.hpp>

#include <chrono>

BOOST_AUTO_TEST_CASE(dot_set_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::set< int, crdt::traits > set(alloc);
	BOOST_ASSERT(set.find(0) == set.end());
	BOOST_ASSERT(set.size() == 0);
	BOOST_ASSERT(set.empty());

	set.insert(1);
	BOOST_ASSERT(set.find(1) != set.end());
	BOOST_ASSERT(set.size() == 1);
	BOOST_ASSERT(!set.empty());

	set.erase(1);
	BOOST_ASSERT(set.find(1) == set.end());
	BOOST_ASSERT(set.size() == 0);
	BOOST_ASSERT(set.empty());

	set.insert(1);
	BOOST_ASSERT(!set.empty());
	set.clear();
	BOOST_ASSERT(set.size() == 0);
	BOOST_ASSERT(set.empty());
}

BOOST_AUTO_TEST_CASE(dot_set_replica_test)
{
	crdt::traits::allocator_type alloc1(1);
	crdt::set< int, crdt::traits > set1(alloc1);

	crdt::traits::allocator_type alloc2(2);
	crdt::set< int, crdt::traits > set2(alloc2);

	set1.insert(1);
	set2.insert(2);

	set1.merge(set2);
	BOOST_ASSERT(set1.size() == 2);
	set2.merge(set1);
	BOOST_ASSERT(set2.size() == 2);
	set2.erase(1);
	set1.merge(set2);
	BOOST_ASSERT(set1.size() == 1);
	set2.clear();
	set1.merge(set2);
	BOOST_ASSERT(set1.size() == 0);
}

BOOST_AUTO_TEST_CASE(dot_map_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::value< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	map.insert(1, crdt::value< int, crdt::traits >(alloc, 1));
	BOOST_ASSERT(map.find(1) != map.end());
	map.erase(1);
	BOOST_ASSERT(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::set< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	//map.insert(1, );
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::value_mv< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_ASSERT(map.find(0) == map.end());
	// map.insert(1, 1);
	// BOOST_ASSERT(map.find(1) != map.end());
	// map.erase(1);
	// BOOST_ASSERT(map.find(1) == map.end());
}

BOOST_AUTO_TEST_CASE(dot_map_map_value_mv_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< 
		int, 
		crdt::map< 
		    int, 
		    crdt::value_mv< int, crdt::traits >, crdt::traits 
		>, 
		crdt::traits
	> map(alloc);

	//BOOST_ASSERT(map.find(0) == false);
	//map.insert(1, 1);
	//BOOST_ASSERT(map.find(1) == true);
	//map.erase(1);
	//BOOST_ASSERT(map.find(1) == false);
}

template < typename Fn > double measure(Fn fn)
{
	using namespace std::chrono;

	auto begin = high_resolution_clock::now();
	fn();
	auto end = high_resolution_clock::now();
	return duration_cast< duration<double> >(end - begin).count();
}

BOOST_AUTO_TEST_CASE(dot_allocator_test)
{
	struct X
	{
		typedef crdt::allocator< uint64_t, char, std::allocator< void > > allocator_type;

		X(allocator_type)
		{}
	};

	crdt::allocator< uint64_t, void, std::allocator< void > > alloc(1);

	std::map< int, X, std::less< int >,
		std::scoped_allocator_adaptor< crdt::allocator< uint64_t, char, std::allocator< void > > >
	> v(alloc);

	X& x = v[1];
}

BOOST_AUTO_TEST_CASE(arena_allocator)
{
	crdt::arena< 1024 > buffer;
	crdt::arena_allocator< int > allocator(buffer);

	auto *p = allocator.allocate(64);
	allocator.deallocate(p, 0);
}

#if !defined(_DEBUG)
BOOST_AUTO_TEST_CASE(dot_test_set_insert_performance)
{
	/*
	auto x = measure([]
	{
		crdt::traits::allocator_type alloc(0);
		crdt::set< size_t, crdt::traits > stdset(alloc);
		for (size_t i = 0; i < 1000000; ++i)
		{
			stdset.test(i);
		}
	});
	*/

	auto t1 = measure([]
	{
		std::set< size_t > stdset;
		for (size_t i = 0; i < 1000000; ++i)
		{
			stdset.insert(i);
		}
	});

	auto t2 = measure([]
	{
		crdt::traits::allocator_type alloc(0);
		crdt::set< size_t, crdt::traits > stdset(alloc);
		for (size_t i = 0; i < 1000000; ++i)
		{
			stdset.insert(i);
		}
	});

	std::cerr << "std::set " << t1 << ", crdt::set " << t2 << std::endl;
}

#endif