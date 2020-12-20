#define NOMINMAX

// TODO: fix
#undef _ENFORCE_MATCHING_ALLOCATORS
#define _ENFORCE_MATCHING_ALLOCATORS 0

#define BOOST_TEST_MODULE header-only multiunit test
#include <boost/test/included/unit_test.hpp>
// #include <boost/test/unit_test.hpp>

#include <fluidstore/crdts/dot.h>

#include <chrono>

BOOST_AUTO_TEST_CASE(dot_set_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::set< int, crdt::traits > set(alloc);
	BOOST_TEST((set.find(0) == set.end()));
	BOOST_TEST(set.size() == 0);
	BOOST_TEST(set.empty());

	set.insert(1);
	BOOST_TEST((set.find(1) != set.end()));
	BOOST_TEST((*set.find(1) == 1));

	BOOST_TEST(set.size() == 1);
	BOOST_TEST(!set.empty());

	set.erase(1);
	BOOST_TEST((set.find(1) == set.end()));
	BOOST_TEST(set.size() == 0);
	BOOST_TEST(set.empty());

	set.insert(1);
	BOOST_TEST(!set.empty());
	set.clear();
	BOOST_TEST(set.size() == 0);
	BOOST_TEST(set.empty());
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
	BOOST_TEST(set1.size() == 2);
	set2.merge(set1);
	BOOST_TEST(set2.size() == 2);
	set2.erase(1);
	set1.merge(set2);
	BOOST_TEST(set1.size() == 1);
	set2.clear();
	set1.merge(set2);
	BOOST_TEST(set1.size() == 0);
}

BOOST_AUTO_TEST_CASE(dot_map_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::value< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_TEST((map.find(0) == map.end()));
	map.insert(1, crdt::value< int, crdt::traits >(alloc, 11));
	BOOST_TEST((map.find(1) != map.end()));
	BOOST_TEST(((*map.find(1)).first == 1));
	BOOST_TEST(((*map.find(1)).second == 11));
	map.erase(1);
	BOOST_TEST((map.find(1) == map.end()));
}

BOOST_AUTO_TEST_CASE(dot_map_set_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::set< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_TEST((map.find(0) == map.end()));
	//map.insert(1, );
	//BOOST_TEST(map.find(1) == true);
	//map.erase(1);
	//BOOST_TEST(map.find(1) == false);
}

BOOST_AUTO_TEST_CASE(dot_map_value_mv_test)
{
	crdt::traits::allocator_type alloc(0);
	crdt::map< int, crdt::value_mv< int, crdt::traits >, crdt::traits > map(alloc);
	BOOST_TEST((map.find(0) == map.end()));
	// map.insert(1, 1);
	// BOOST_TEST(map.find(1) != map.end());
	// map.erase(1);
	// BOOST_TEST(map.find(1) == map.end());
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

	//BOOST_TEST(map.find(0) == false);
	//map.insert(1, 1);
	//BOOST_TEST(map.find(1) == true);
	//map.erase(1);
	//BOOST_TEST(map.find(1) == false);
}

template < typename Fn > double measure(Fn fn)
{
	using namespace std::chrono;

	auto begin = high_resolution_clock::now();
	fn();
	auto end = high_resolution_clock::now();
	return duration_cast< duration<double> >(end - begin).count();
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

	#define Outer 10000
	#define Inner 100

	auto t1 = measure([]
	{
		for (size_t x = 0; x < Outer; ++x)
		{
			std::set< size_t > stdset;
			for (size_t i = 0; i < Inner; ++i)
			{
				stdset.insert(i);
			}
		}
	});

	auto t2 = measure([]
	{
		for (size_t x = 0; x < Outer; ++x)
		{
			crdt::traits::allocator_type alloc(0);
			crdt::set< size_t, crdt::traits > stdset(alloc);
			for (size_t i = 0; i < Inner; ++i)
			{
				stdset.insert(i);
			}
		}
	});

	std::cerr << "std::set " << t1 << ", crdt::set " << t2 << ", slowdown " << t2/t1 << std::endl;
}

#endif