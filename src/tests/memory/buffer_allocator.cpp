#include <boost/test/unit_test.hpp>

#include <fluidstore/memory/buffer_allocator.h>

BOOST_AUTO_TEST_CASE(dynamic_buffer)
{
	memory::dynamic_buffer< std::allocator< void > > buffer(1024);
	BOOST_TEST(buffer.get_allocated() == 0);

	memory::buffer_allocator< uint8_t, decltype(buffer) > allocator(buffer);

	auto p = allocator.allocate(1);
	BOOST_TEST(buffer.get_allocated() > 0);
	allocator.deallocate(p, 1);
	BOOST_TEST(buffer.get_allocated() == 0);
}

BOOST_AUTO_TEST_CASE(static_buffer)
{
	memory::static_buffer< 1024 > buffer;
	BOOST_TEST(buffer.get_allocated() == 0);

	memory::buffer_allocator< uint8_t, decltype(buffer) > allocator(buffer);

	auto p = allocator.allocate(1);
	BOOST_TEST(buffer.get_allocated() > 0);
	allocator.deallocate(p, 1);
	BOOST_TEST(buffer.get_allocated() == 0);
}