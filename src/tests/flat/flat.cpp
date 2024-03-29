#include <fluidstore/flat/vector.h>
#include <fluidstore/flat/set.h>
#include <fluidstore/flat/map.h>
#include <fluidstore/crdt/detail/dot.h>
#include <fluidstore/memory/buffer_allocator.h>

#include <boost/test/unit_test.hpp>
#include <deque>

using namespace crdt::flat;

BOOST_AUTO_TEST_CASE(vector_basic_operations)
{
    std::allocator< int > allocator;
    
    vector_base< int > vec;
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

    vec.erase(allocator, vec.find(1));
    BOOST_TEST(vec.size() == 2);
    vec.erase(allocator, vec.find(0));
    BOOST_TEST((*vec.begin() == 2));
    BOOST_TEST(vec.size() == 1);

    vec.clear(allocator);
}

BOOST_AUTO_TEST_CASE(set_basic_operations)
{
    std::allocator< int > allocator;

    set_base< int > set;
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

BOOST_AUTO_TEST_CASE(map_basic_operations2)
{
    std::allocator< int > allocator;

    map_base< int, int > map;
    map.emplace(allocator, 3, 3);
    BOOST_TEST((map.find(3) != map.end()));
    BOOST_TEST((++map.find(3) == map.end()));
    
    map.emplace(allocator, 1, 1);
    BOOST_TEST((map.find(1) != map.end()));
    BOOST_TEST((++map.find(1) == map.find(3)));
    BOOST_TEST((++map.find(3) == map.end()));

    map.emplace(allocator, 2, 2);
    BOOST_TEST((++map.find(2) == map.find(3)));
    
    map.clear(allocator);
}

BOOST_AUTO_TEST_CASE(map_custom_node)
{
    std::allocator< int > allocator;

    struct node
    {
        node(std::allocator_arg_t, std::allocator< int >&, int key, int)
        //node(int key, int)
            : first(key)
        {}

        bool operator < (const int& other) const { return first < other; }
        bool operator < (const node& other) const { return first < other.first; }
        
        bool operator == (const int& other) const { return first == other; }
        bool operator == (const node& other) const { return first == other.first; }

        const int first;
    };

    map_base< int, int, node > map;
    map.emplace(allocator, node(std::allocator_arg, allocator, 1, 1));
    map.clear(allocator);
}

BOOST_AUTO_TEST_CASE(vector_string)
{
    memory::static_buffer< 8192 > buffer;
    memory::buffer_allocator< void, decltype(buffer) > allocator(buffer);

    {
        vector_base< std::string > vec;
        for (size_t i = 0; i < 10; ++i)
        {
            vec.push_back(allocator, std::to_string(i));
        }

        while (!vec.empty())
        {
            vec.erase(allocator, vec.begin());
        }

        vec.clear(allocator);
    }

    BOOST_TEST(buffer.get_allocated() == 0);

    {
        vector_base< std::string > vec;
        for (size_t i = 0; i < 10; ++i)
        {
            vec.push_back(allocator, std::to_string(i));
        }

        while (!vec.empty())
        {
            vec.erase(allocator, --vec.end());
        }

        vec.clear(allocator);
    }

    BOOST_TEST(buffer.get_allocated() == 0);
}

BOOST_AUTO_TEST_CASE(set_string)
{
    /*
    crdt::arena< 32768 > arena;
    crdt::arena_allocator< int > allocator(arena);

    {
        vector_base< std::string > vec;
        for (size_t i = 0; i < 10; ++i)
        {
            vec.push_back(allocator, std::to_string(i));
        }
    }
    */
}

BOOST_AUTO_TEST_CASE(map_non_default)
{
    std::allocator< int > allocator;

    struct data 
    {
        using allocator_type = decltype(allocator);

        data(decltype(allocator)) {}
        data(data&&) {}

        data& operator = (data&&)
        {
            return *this;
        }
    };

    map< int, data, decltype(allocator) > map(allocator);
    // map[1];
}

BOOST_AUTO_TEST_CASE(vector_dots_type)
{
    std::allocator< int > allocator;

    crdt::flat::map_base< crdt::dot< int, int >, int > dots;
    
    dots.emplace(allocator, crdt::dot< int, int >{1,1}, 1);
    dots.emplace(allocator, crdt::dot< int, int >{1,2}, 1);

    dots.clear(allocator);
}

BOOST_AUTO_TEST_CASE(vector_size_type)
{
    std::allocator< char > allocator;
    crdt::flat::vector_base< char, unsigned char > vector;

    BOOST_TEST(vector.max_size() == std::numeric_limits< unsigned char >::max());
    for (auto i = 0; i < vector.max_size(); ++i)
    {
        vector.push_back(allocator, static_cast<char>(i));
    }

    BOOST_CHECK_THROW(vector.push_back(allocator, 0), std::runtime_error);
    vector.clear(allocator);
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

const int loops = 10000;
#if defined(_DEBUG)
const int elements = 1024;
#else
const int elements = 128;
#endif

/*
BOOST_AUTO_TEST_CASE(std_set_insert_performance)
{
    {
        crdt::arena< 32768 > arena;
        crdt::arena_allocator< int > allocator(arena);

        for (int count = 1; count <= elements; count *= 2)
        {
            auto t = measure(loops, [&]
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

        for (int count = 1; count <= elements; count *= 2)
        {
            std::allocator< int > allocator;
            auto t = measure(loops, [&]
            {
                set_base< int > set;
                for (int i = 0; i < count; ++i)
                {
                    set.emplace(allocator, i);
                }

                set.clear(allocator);
            });

            std::cerr << "flat::set count " << count << ", time " << t << std::endl;
        }
    }
}
*/

#if 0
BOOST_AUTO_TEST_CASE(flat_vector_append_performance)
{
    {
        crdt::arena< 32768*4 > arena;
        crdt::arena_allocator< int > allocator(arena);
        //std::allocator< int > allocator;

        for (int count = 1; count <= elements; count *= 2)
        {
            auto t = measure(loops, [&]
            {
                vector_base< int > vec;
                for (int i = 0; i < count; ++i)
                {
                    vec.push_back(allocator, i);
                }

                vec.clear(allocator);
            });

            std::cerr << "flat::vector count " << count << ", time " << t << std::endl;
        }
    }

    {
        crdt::arena< 32768*4 > arena;
        crdt::arena_allocator< int > allocator(arena);
        //std::allocator< int > allocator;

        for (int count = 1; count <= elements; count *= 2)
        {
            std::allocator< int > allocator;
            auto t = measure(loops, [&]
            {
                hat_base< int > hat;
                for (int i = 0; i < count; ++i)
                {
                    hat.push_back(allocator, i);
                }

                hat.clear(allocator);
            });

            std::cerr << "flat::hat count " << count << ", time " << t << std::endl;
        }
    }

    /*
    {
        crdt::arena< 32768 * 4 > arena;
        //crdt::arena_allocator< int > allocator(arena);
        std::allocator< int > allocator;

        for (int count = 1; count <= elements; count *= 2)
        {
            auto t = measure(loops, [&]
            {
                std::deque< int > deq;
                for (int i = 0; i < count; ++i)
                {
                    deq.push_back(i);
                }
            });

            std::cerr << "std::deque count " << count << ", time " << t << std::endl;
        }
    }
    */
}

#endif

#define PRINT_SIZEOF(...) std::cerr << "sizeof " << # __VA_ARGS__ << ": " << sizeof(__VA_ARGS__) << std::endl

BOOST_AUTO_TEST_CASE(flat_sizeof)
{
    PRINT_SIZEOF(vector_base< int >);
}