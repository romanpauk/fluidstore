#include <sqlstl/set.h>

void set_test()
{
    sqlstl::db db(":memory:");
    sqlstl::allocator allocator(db);
    sqlstl::set< int, sqlstl::allocator > set(allocator, "Set");

    assert(set.size() == 0);

    {
        auto it = set.insert(1);
        assert(it.first == set.find(1));
        assert(*it.first == 1);
        assert(it.first != set.find(2));
        assert(it.second == true);
    }

    {
        auto it = set.insert(1);
        assert(it.second == false);
        assert(*it.first == 1);
    }

    assert(set.size() == 1);
    
    {
        auto it = set.insert(2);
        assert(set.find(2) == it.first);
    }

    assert(set.size() == 2);

    {
        int count = 0;
        for (auto&& value : set)
        {
            assert(value == ++count);
        }
        assert(count == 2);
    }
}

