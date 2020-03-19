#include <crdt/set_g.h>
#include <crdt/traits.h>

template < typename Traits > void set_g_test_impl(typename Traits::Allocator& allocator)
{
    crdt::set_g< int, Traits > set(allocator, "set");
    assert(set.size() == 0);
    {
        auto it = set.insert(1);
        assert(*it.first == 1);
        assert(it.second == true);
    }
    {
        auto it = set.insert(1);
        assert(*it.first == 1);
        assert(it.second == false);
    }
    assert(set.size() == 1);
    assert(set.find(1) != set.end());
    set.insert(2);
    assert(set.size() == 2);
    assert(set.find(2) != set.end());

    int count = 0;
    for(auto&& value: set)
    {
        ++count;
        assert(value == count);
    }

    assert(count == 2);

    // iterator based insert, pairb

    crdt::set_g< int, Traits > set2 = std::move(set);
    assert(set2.size() == 2);
    std::optional< crdt::set_g< int, Traits > > opt(std::move(set2));
    std::optional< std::pair< const int, crdt::set_g< int, Traits > > > opt2({ 1, std::move(set2) });

}

void set_g_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::allocator allocator(db);

        set_g_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator allocator;
        set_g_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}