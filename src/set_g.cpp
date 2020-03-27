#include <boost/test/unit_test.hpp>

#include <crdt/set_g.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void set_g_test_impl(typename Allocator allocator)
{
    crdt::set_g< int, Traits > set(Allocator(allocator, "set"));
    BOOST_ASSERT(set.size() == 0);
    {
        auto it = set.insert(1);
        BOOST_ASSERT(*it.first == 1);
        BOOST_ASSERT(it.second == true);
    }
    {
        auto it = set.insert(1);
        BOOST_ASSERT(*it.first == 1);
        BOOST_ASSERT(it.second == false);
    }
    BOOST_ASSERT(set.size() == 1);
    BOOST_ASSERT(set.find(1) != set.end());
    set.insert(2);
    BOOST_ASSERT(set.size() == 2);
    BOOST_ASSERT(set.find(2) != set.end());

    int count = 0;
    for(auto&& value: set)
    {
        ++count;
        BOOST_ASSERT(value == count);
    }

    BOOST_ASSERT(count == 2);

    // iterator based insert, pairb

    crdt::set_g< int, Traits > set2 = std::move(set);
    BOOST_ASSERT(set2.size() == 2);
    std::optional< crdt::set_g< int, Traits > > opt(std::move(set2));
    std::optional< std::pair< const int, crdt::set_g< int, Traits > > > opt2({ 1, std::move(set2) });

}

BOOST_AUTO_TEST_CASE(set_g_test)
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);

        set_g_test_impl< crdt::traits< crdt::sqlite > >(sqlstl::allocator<void>(factory));
    }

    {
        crdt::allocator<void> allocator;
        set_g_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}