#include <sqlstl/sqlite.h>
#include <sqlstl/map.h>
#include <sqlstl/factory.h>

void map_test()
{
    sqlstl::db db(":memory:");
    sqlstl::factory factory(db);
    sqlstl::map< int, int, sqlstl::factory > map(factory, "Map");

    assert(map.size() == 0);

    map[1] = 10;
    {
        auto it = map.find(1);
        assert((*it).first == 1);
        assert((*it).second == 10);
        assert(++it == map.end());
        assert(map.size() == 1);
    }

    map[2] = 20;
    {
        auto it = map.find(2);
        assert((*it).first == 2);
        assert((*it).second == 20);
        assert(++it == map.end());
        assert(map.size() == 2);
    }

    {
        int count = 0;
        for (auto&& value : map)
        {
            assert(value.first == ++count);
            assert(value.second == count * 10);
        }
        assert(count == 2);
    }

    sqlstl::map< int, sqlstl::map< int, int, sqlstl::factory >, sqlstl::factory > nested(factory, "Map2");
    nested[1][10] = 100;
    nested[2][20] = 200;

    {
        int count = 0;
        for (auto&& value : nested)
        {
            assert(value.first == ++count);
            assert((*value.second.find(count * 10)).second == count * 100);
        }
        assert(count == 2);
    }
}
