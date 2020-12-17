#include <crdt/dot.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(dot_test)
{
	crdt::set< uint64_t, uint64_t, int > keys;
	BOOST_ASSERT(keys.find(0) == false);
	keys.insert(1);
	BOOST_ASSERT(keys.find(1) == true);
	keys.remove(1);
	BOOST_ASSERT(keys.find(1) == false);
}