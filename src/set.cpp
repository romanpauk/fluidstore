#include <sqlstl/set.h>

void set_test()
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);
    sqlstl::set< int, sqlstl::factory > set(factory, "Set");

    assert(set.size() == 0);

    {
        auto it = set.insert(1);
        assert(it.first == set.find(1));
        assert(it.first != set.find(2));
        assert(it.second == true);
    }

    {
        auto it = set.insert(1);
        assert(it.second == false);
    }

    assert(set.size() == 1);
    
    {
        auto it = set.insert(2);
        assert(it.first == set.find(2));
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

