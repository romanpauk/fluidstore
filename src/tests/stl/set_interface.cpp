#include <fluidstore/btree/set.h>

#include <boost/test/unit_test.hpp>

#include "stl/set_interface.h"

typedef boost::mpl::list < 
	std::set< int >, 
	btree::set< int >
> container_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(set_typedefs, ContainerType, container_types)
{
	set_interface< ContainerType >::typedefs();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_constructors, ContainerType, container_types)
{
	set_interface< ContainerType >::constructor_default();
	set_interface< ContainerType >::constructor_allocator(typename ContainerType::allocator_type());
}

typedef boost::mpl::list < btree::set< std::tuple< int, int > > > container_types_emplace;
BOOST_AUTO_TEST_CASE_TEMPLATE(set_emplace, ContainerType, container_types_emplace)
{
	set_interface< ContainerType >::emplace(1, 1);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_find, ContainerType, container_types)
{
	set_interface< ContainerType >::find_iterator();
	set_interface< ContainerType >::find_const_iterator();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_empty, ContainerType, container_types_emplace)
{
	set_interface< ContainerType >::empty();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_insert, ContainerType, container_types)
{
	set_interface< ContainerType >::insert_const_lvalue();
	set_interface< ContainerType >::insert_rvalue();
	set_interface< ContainerType >::insert_hint_const_lvalue();
	set_interface< ContainerType >::insert_hint_rvalue();
	
	// TODO
	// set_interface::insert_const_hint_const_lvalue();
	// set_interface::insert_const_hint_rvalue();

	set_interface< ContainerType >::insert_range();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_iterators, ContainerType, container_types)
{
	set_interface< ContainerType >::iterator_begin();
	set_interface< ContainerType >::iterator_begin_const();
	set_interface< ContainerType >::iterator_end();
	set_interface< ContainerType >::iterator_end_const();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_size, ContainerType, container_types)
{
	set_interface< ContainerType >::size();
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_lower_bound, ContainerType, container_types)
{
	set_interface< ContainerType >::lower_bound_iterator();
	set_interface< ContainerType >::lower_bound_const_iterator();
}