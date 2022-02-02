#include <boost/test/unit_test.hpp>

#include <fluidstore/btree/set.h>
#include "stl/set_interface.h"

typedef boost::mpl::list <
    btree::set< std::tuple< int, int > >
> container_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_interface_suite, ContainerType, container_types)
{
    ContainerType container;

    set_interface< ContainerType >::constructor_default();
    set_interface< ContainerType >::constructor_allocator(typename ContainerType::allocator_type());
    set_interface< ContainerType >::emplace(container, 1, 1);
    set_interface< ContainerType >::test_suite(container);
}