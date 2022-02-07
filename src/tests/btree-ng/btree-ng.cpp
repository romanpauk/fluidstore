#include <boost/test/unit_test.hpp>

#include <fluidstore/btree-ng/btree.h>

BOOST_AUTO_TEST_CASE(btreeng_tagged_ptr)
{
	uintptr_t val;
	uintptr_t* p = &val;
	assert(btreeng::get_ptr(p) == p);
	assert(btreeng::get_tag(p) == 0);
	btreeng::set_tag(p, 1);
	assert(btreeng::get_tag(p) == 1);
	assert(btreeng::get_ptr(p) == &val);
}

BOOST_AUTO_TEST_CASE(btreeng_index_node)
{
	btreeng::index_node< uint32_t, 7 > node;
	static_assert(sizeof(node) == 64);
}

BOOST_AUTO_TEST_CASE(btreeng_value_node)
{
	btreeng::value_node< uint32_t, 8 > node;
	static_assert(sizeof(node) == 64);
}


BOOST_AUTO_TEST_CASE(btreeng_btree_array_traits_uint32_t_8_find_key_index)
{
	uint32_t key = 12;
	alignas(32) uint32_t keys[8] = { 0, 1, 12, 13, 14, 15, 16, 17 };

	/*
	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 0);
		assert(result == 0);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 1);
		assert(result == 1);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 2);
		assert(result == 2);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 9);
		assert(result == 2);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 12);
		assert(result == 2);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 8, 100);
		assert(result == 8);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 4, 100);
		assert(result == 4);
	}

	{
		volatile uint8_t result = btreeng::array_traits< uint32_t, 8 >::count_lt(keys, 2, 13);
		assert(result == 2);
	}
	*/
}

BOOST_AUTO_TEST_CASE(btreeng_btree)
{
	static_assert(sizeof(btreeng::btree< uint32_t >) == 32);

	btreeng::btree< uint32_t > c{};
	
	static_assert(sizeof(btreeng::btree_static_node< uint32_t >) == 32);
	static_assert(sizeof(btreeng::btree_dynamic_node) == 32);
	
	assert(c.size() == 0);
	c.insert(1);
	assert(c.size() == 1);
	c.insert(3);
	assert(c.size() == 2);
	c.insert(2);
	assert(c.size() == 3);
	c.insert(2);
	assert(c.size() == 3);

	c.insert(4);
	c.insert(5);
	c.insert(6);
	c.insert(7);
	// value_node
	c.insert(8);
	assert(c.size() == 8);
	c.insert(9);
	assert(c.size() == 9);
}
