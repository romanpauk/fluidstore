#include <fluidstore/flat/vector.h>
#include <fluidstore/flat/set.h>

#include <fluidstore/allocators/arena_allocator.h>

#include <boost/test/unit_test.hpp>

using namespace crdt::flat;

BOOST_AUTO_TEST_CASE(vector_basic_operations)
{
    crdt::arena< 32768 > arena;
    crdt::arena_allocator< int > allocator(arena);

    vector< int > vec;
    BOOST_TEST(vec.size() == 0);
    BOOST_TEST(vec.empty());
    BOOST_TEST((vec.find(1) == vec.end()));
    BOOST_TEST((vec.begin() == vec.end()));

    vec.push_back(allocator, 1);
    BOOST_TEST(vec.size() == 1);
    BOOST_TEST(!vec.empty());
    BOOST_TEST((vec.find(1) != vec.end()));
    BOOST_TEST((++vec.find(1) == vec.end()));
    
    vec.push_back(allocator, 2);
    BOOST_TEST(vec.size() == 2);
    BOOST_TEST((vec.find(1) != vec.end()));
    BOOST_TEST((++vec.find(1) == vec.find(2)));

    vec.emplace(allocator, vec.begin(), 0);
    BOOST_TEST((++vec.find(0) == vec.find(1)));
    BOOST_TEST((++vec.find(1) == vec.find(2)));
    BOOST_TEST((++vec.find(2) == vec.end()));

    vec.clear(allocator);
}

BOOST_AUTO_TEST_CASE(set_basic_operations)
{
    crdt::arena< 32768 > arena;
    crdt::arena_allocator< int > allocator(arena);

    set< int > set;
    set.emplace(allocator, 3);
    BOOST_TEST((set.find(3) != set.end()));
    BOOST_TEST((++set.find(3) == set.end()));

    set.emplace(allocator, 1);
    BOOST_TEST((set.find(1) != set.end()));
    BOOST_TEST((++set.find(1) == set.find(3)));
    BOOST_TEST((++set.find(3) == set.end()));

    set.emplace(allocator, 2);
    BOOST_TEST((++set.find(2) == set.find(3)));

    set.clear(allocator);
}

template < typename Fn > double measure(int count, Fn fn)
{
    using namespace std::chrono;
    fn();
    auto begin = high_resolution_clock::now();
    for (size_t i = 0; i < count; ++i)
    {
        fn();
    }
    auto end = high_resolution_clock::now();
    return duration_cast<duration<double>>(end - begin).count();
}

template < typename Container > void test_insert(Container& container, size_t elements)
{
    for (size_t i = 0; i < elements; ++i)
    {
        container.insert(i);
    }
}

BOOST_AUTO_TEST_CASE(std_set_insert_performance)
{
    {
        crdt::arena< 32768 > arena;
        crdt::arena_allocator< int > allocator(arena);

        for (int count = 1; count <= 8192; count *= 2)
        {
            auto t = measure(1000, [&]
            {
                // std::set< int, std::less< int >, decltype(allocator) > set(allocator);
                std::set< int > set;
                for (int i = 0; i < count; ++i)
                {
                    set.insert(i);
                }
            });

            std::cerr << "std::set count " << count << ", time " << t << std::endl;
        }
    }
 
    {
        crdt::arena< 32768 > arena;
        crdt::arena_allocator< int > allocator(arena);

        for (int count = 1; count <= 8192; count *= 2)
        {
            std::allocator< int > allocator;
            auto t = measure(1000, [&]
            {
                set< int > set;
                for (int i = 0; i < count; ++i)
                {
                    set.emplace(allocator, i);
                }

                set.clear(allocator);
            });

            std::cerr << "crdt::set count " << count << ", time " << t << std::endl;
        }
    }
}