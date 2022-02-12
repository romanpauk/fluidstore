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
	static_assert(sizeof(btreeng::btree< uint32_t >) <= 64);

	static_assert(sizeof(btreeng::btree_static_node< uint32_t >) == 32);
	static_assert(sizeof(btreeng::btree_dynamic_node) == 32);

}

const int ElementCount = 10000;

BOOST_AUTO_TEST_CASE(btreeng_btree_insert)
{
	volatile bool inserted;

	btreeng::btree< uint32_t > c{};
	assert(c.size() == 0);
	assert(c.find(0) == false);

	size_t cnt = 0;
	for (size_t i = 0; i < ElementCount; ++i)
	{
		auto pb = c.insert(i);
		++cnt;
		inserted = pb.second;

		volatile auto size = c.size();
		assert(size == cnt);

		inserted = c.find(i);
		assert(inserted == true);

		//*
		for (size_t j = 1; j < i; ++j)
		{
			inserted = c.find(j);
			if (!inserted)
				std::abort();
			assert(inserted);
		}
		//*/
	}
}

BOOST_AUTO_TEST_CASE(btreeng_btree_backward)
{
	volatile bool inserted;

	btreeng::btree< uint32_t > c{};
	assert(c.size() == 0);
	assert(c.find(0) == false);

	size_t cnt = 0;
	for (size_t i = ElementCount; i >= 1; --i)
	{
		auto pb = c.insert(i);
		++cnt;
		inserted = pb.second;

		volatile auto size = c.size();
		assert(size == cnt);

		inserted = c.find(i);
		assert(inserted == true);

		//*
		for (size_t j = ElementCount; j >= i; --j)
		{
			if (j == 10000)
			{
				int a(1);
			}

			inserted = c.find(j);
			if (!inserted)
				std::abort();
			assert(inserted);
		}
		//*/
	}
}
