#include <boost/test/unit_test.hpp>

#include <fluidstore/memory/buffer_allocator.h>

template < typename Allocator, typename Buffer > void test_minimal_size(Allocator& allocator, Buffer& buffer)
{
	BOOST_TEST(buffer.get_allocated() == 0);
	auto p = allocator.allocate(1);
	BOOST_TEST(buffer.get_allocated() > 0);
	allocator.deallocate(p, 1);
	BOOST_TEST(buffer.get_allocated() == 0);
	BOOST_CHECK_THROW(allocator.allocate(2), std::bad_alloc);
}

BOOST_AUTO_TEST_CASE(static_buffer_minimal)
{
	memory::static_buffer< sizeof(uint8_t) > buffer;
	memory::buffer_allocator< uint8_t, decltype(buffer) > allocator(buffer);
	test_minimal_size(allocator, buffer);
}

BOOST_AUTO_TEST_CASE(dynamic_buffer_minimal)
{
	memory::dynamic_buffer buffer(sizeof(uint8_t));
	memory::buffer_allocator< uint8_t, decltype(buffer) > allocator(buffer);
	test_minimal_size(allocator, buffer);
}

BOOST_AUTO_TEST_CASE(static_buffer_fallback)
{
	memory::static_buffer< sizeof(uint8_t) > buffer;
	memory::buffer_allocator< uint8_t, decltype(buffer), std::allocator< uint8_t > > allocator(buffer);
	auto p1 = allocator.allocate(1);
	auto p2 = allocator.allocate(1);
	allocator.deallocate(p2, 1);
	allocator.deallocate(p1, 1);
}

BOOST_AUTO_TEST_CASE(static_dynamic_buffer)
{	
	// TODO: dynamic buffer needs to be lazy ;)
	memory::dynamic_buffer heap(sizeof(uint8_t));
	memory::buffer_allocator< uint8_t, decltype(heap) > heap_allocator(heap);

	memory::static_buffer< sizeof(uint8_t) > stack;
	memory::buffer_allocator< uint8_t, decltype(stack), decltype(heap_allocator) > allocator(stack, heap_allocator);

	auto stack_p = allocator.allocate(1);
	BOOST_TEST(stack.get_allocated() > 0);
	
	auto heap_p = allocator.allocate(1);
	BOOST_TEST(heap.get_allocated() > 0);

	BOOST_CHECK_THROW(allocator.allocate(1), std::bad_alloc);

	allocator.deallocate(stack_p, 1);
	BOOST_TEST(stack.get_allocated() == 0);

	allocator.deallocate(heap_p, 1);
	BOOST_TEST(heap.get_allocated() == 0);
}
