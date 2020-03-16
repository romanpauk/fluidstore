#include <crdt/set_or.h>
#include <crdt/traits.h>

template < typename Traits > void set_or_test_impl(typename Traits::Allocator& allocator)
{
    crdt::set_or< int, Traits > set(allocator, "set");
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
    for (auto&& value : set)
    {
        ++count;
        assert(value == count);
    }

    assert(count == 2);

    set.erase(2);
    assert(set.find(2) == set.end());

    count = 0;
    for (auto&& value : set)
    {
        ++count;
        assert(value == count);
    }
    assert(count == 1);
}

void set_or_test()
{
    {
        sqlstl::db db(":memory:");
        sqlstl::allocator allocator(db);

        set_or_test_impl< crdt::traits< crdt::sqlite > >(allocator);
    }

    {
        crdt::allocator allocator;
        set_or_test_impl< crdt::traits< crdt::memory > >(allocator);
    }
}