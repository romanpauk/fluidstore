#include <boost/test/unit_test.hpp>

#include <crdt/set_or.h>
#include <crdt/traits.h>

template < typename Traits, typename Allocator > void set_or_test_impl(typename Allocator allocator)
{
    crdt::set_or< int, Traits > set(Allocator(allocator, "set"));
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
    for (auto&& value : set)
    {
        ++count;
        BOOST_ASSERT(value == count);
    }

    BOOST_ASSERT(count == 2);

    set.erase(2);
    BOOST_ASSERT(set.find(2) == set.end());

    count = 0;
    for (auto&& value : set)
    {
        ++count;
        BOOST_ASSERT(value == count);
    }
    BOOST_ASSERT(count == 1);

    crdt::set_or< int, Traits > set2 = std::move(set);
    BOOST_ASSERT(set2.size() == 1);

    typename crdt::set_or< int, Traits >::set_or_tags tags(Allocator(allocator, "tags"));
    std::optional< std::pair< int, typename crdt::set_or< int, Traits >::set_or_tags > > tagsopt(std::make_pair(1, std::move(tags)));
}

BOOST_AUTO_TEST_CASE(set_or_test)
{
    {
        sqlstl::db db(":memory:");
        sqlstl::factory factory(db);

        set_or_test_impl< crdt::traits< crdt::sqlite > >(sqlstl::allocator<void>(factory));
    }

    {
        crdt::allocator<void> allocator;
        set_or_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}