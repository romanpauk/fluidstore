#include <fluidstore/btree/btree.h>
#include <fluidstore/flat/set.h>
#include <fluidstore/allocators/arena_allocator.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <boost/container/flat_set.hpp>
#include <iomanip>

#include <windows.h>
#include <profileapi.h>

//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
//_CrtSetBreakAlloc(6668782);

static int Iters = 100;
static int Max = 32768;
static const int ArenaSize = 65536 * 2;

template < typename T, size_t N > struct descriptor
{
    using size_type = size_t;

    descriptor()
        : size_()
    {}

    size_type size() const { return size_; }
    size_type capacity() const { return N; }

    void set_size(size_type size)
    {
        assert(size <= capacity());
        size_ = size;
    }

    T* data() { return reinterpret_cast<T*>(data_); }
    const T* data() const { return reinterpret_cast<const T*>(data_); }

private:
    uint8_t data_[sizeof(T) * N];
    size_type size_;
};

BOOST_AUTO_TEST_CASE(btree_fixed_vector)
{
    std::allocator< char > a;
    btree::fixed_vector < std::string, descriptor < std::string, 8 > > c((descriptor < std::string, 8 >()));
    c.emplace_back(a, "2");
    c.emplace_back(a, "3");
    c.emplace(a, c.begin(), "1");
    c.clear(a);
}

BOOST_AUTO_TEST_CASE(btree_fixed_split_vector)
{
    std::allocator< char > a;
    
    btree::fixed_vector < std::string, descriptor < std::string, 8 > > v1((descriptor < std::string, 8 >()));
    btree::fixed_vector < std::string, descriptor < std::string, 8 > > v2((descriptor < std::string, 8 >()));

    btree::fixed_split_vector < 
        btree::fixed_vector < std::string, descriptor < std::string, 8 > >,
        btree::fixed_vector < std::string, descriptor < std::string, 8 > >
    > c(v1, v2);

    btree::fixed_vector < std::string, descriptor < std::string, 8 > > v3((descriptor < std::string, 8 >()));
    btree::fixed_vector < std::string, descriptor < std::string, 8 > > v4((descriptor < std::string, 8 >()));

    btree::fixed_split_vector <
        btree::fixed_vector < std::string, descriptor < std::string, 8 > >,
        btree::fixed_vector < std::string, descriptor < std::string, 8 > >
    > c2(v3, v4);

    c.size();
    c.clear(a);
    c.begin();
    //c.erase(a, c.begin());
    c.emplace_back(a, std::make_tuple("a", "b"));
    auto x = c[0];

    c2.emplace_back(a, std::make_tuple("c", "d"));

    c.insert(a, c.begin(), c2.begin(), c2.end());
    auto y = c[0];

    c.clear(a);
    c2.clear(a);
}

template < typename T > T value(size_t);

template <> uint8_t value<uint8_t>(size_t i) { return static_cast<uint8_t>(i); }
template <> uint16_t value<uint16_t>(size_t i) { return static_cast<uint16_t>(i); }
template <> uint32_t value<uint32_t>(size_t i) { return static_cast<uint32_t>(i);; }
template <> uint64_t value<uint64_t>(size_t i) { return static_cast<uint64_t>(i);; }
template <> std::string value<std::string>(size_t i) { return std::to_string(i); }

typedef boost::mpl::list < uint8_t, uint16_t, uint32_t, uint64_t, std::string > test_types;

BOOST_AUTO_TEST_CASE(btree_set_node_dimension)
{
    // Note: Total number of elements in node is N * 2

    typedef btree::set< uint8_t > set_uint8_t;
    BOOST_TEST(set_uint8_t::dimension == 32);

    typedef btree::set< uint16_t > set_uint16_t;
    BOOST_TEST(set_uint16_t::dimension == 16);

    typedef btree::set< uint32_t > set_uint32_t;
    BOOST_TEST(set_uint32_t::dimension == 8);

    typedef btree::set< uint64_t > set_uint64_t;
    BOOST_TEST(set_uint64_t::dimension == 4);

    typedef btree::set< std::string > set_string;
    BOOST_TEST(set_string::dimension == 4);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_insert, T, test_types)
{
    btree::set< T > c;
    BOOST_TEST(c.size() == 0);
    BOOST_TEST(c.empty());
    BOOST_TEST((c.find(value<T>(0)) == c.end()));

    {
        auto pairb = c.insert(value<T>(2));
        BOOST_TEST(pairb.second == true);
        BOOST_TEST(*pairb.first == value<T>(2));
        BOOST_TEST((c.find(value<T>(2)) == pairb.first));
        BOOST_TEST((c.find(value<T>(2)) != c.end()));

        BOOST_TEST(c.size() == 1);
        BOOST_TEST(c.insert(value<T>(2)).second == false);
        BOOST_TEST(c.size() == 1);
    }

    c.insert(value<T>(1));
    BOOST_TEST(c.size() == 2);
    BOOST_TEST((c.find(value<T>(1)) != c.end()));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_insert, T, test_types)
{
    btree::map< int, T > c;
    c.insert(std::make_pair(1, value<T>(10)));
    BOOST_TEST((*c.find(1)).second == value<T>(10));

    // TODO
}

typedef boost::mpl::list<size_t> btree_range_for_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_range_for, T, btree_range_for_types)
{
    for (int i = 0; i < 1000; ++i)
    {
        btree::set< T > c;

        for (int j = 0; j < i; ++j)
        {
            c.insert(value<T>(j));
        }

        int k = 0;
        for (auto& v : c)
        {
            BOOST_REQUIRE(v == value<T>(k++));
        }

        BOOST_TEST(k == c.size());
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_insert_loop, T, test_types)
{
    const int N = 1000;
    if ((1ull << sizeof(T)) < N)
    {
        return;
    }

    btree::set< T > c;
    for (int i = 0; i < N; ++i)
    {
        auto iv = value<T>(i);
        BOOST_REQUIRE(c.insert(iv).second);
        BOOST_REQUIRE((c.find(iv) != c.end()));
        BOOST_REQUIRE(*c.find(iv) == iv);

        // Check that the tree was not damaged by insertion
        for (int j = 0; j < i; ++j)
        {
            auto jv = value<T>(j);
            BOOST_REQUIRE((c.find(jv) != c.end()));
            BOOST_REQUIRE(*c.find(jv) == jv);
        }
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_erase_loop, T, test_types)
{
//#if defined(_DEBUG)
    const int N = 100;
//#else
//    const int N = 1000;
//#endif

    if ((1ull << sizeof(T)) < N)
    {
        return;
    }

    for (int i = 0; i < N; ++i)
    {
        btree::set< T > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert(value<T>(j));
        }

        for (int j = 0; j < i; ++j)
        {
            auto jv = value<T>(j);
            c.erase(jv);
            BOOST_REQUIRE((c.find(jv) == c.end()));

            for (int k = j + 1; k < i; ++k)
            {
                BOOST_REQUIRE(c.find(value<T>(k)) != c.end());
            }
        }
    }

    for (int i = 0; i < N; ++i)
    {
        btree::set< T > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert(value<T>(j));
        }

        for (int j = i - 1; j > 0; --j)
        {
            auto jv = value<T>(j);
            c.erase(jv);
            BOOST_REQUIRE((c.find(jv) == c.end()));

            for (int k = j - 1; k > 0; --k)
            {
                BOOST_REQUIRE(c.find(value<T>(k)) != c.end());
            }
        }
    }
}

#if !defined(_DEBUG)

template < typename Fn > double measure(size_t loops, Fn&& fn)
{
    // https://uwaterloo.ca/embedded-software-group/sites/ca.embedded-software-group/files/uploads/files/ieee-esl-precise-measurements.pdf

    using namespace std::chrono;

    double total = 0;
    for (size_t i = 0; i < loops; ++i)
    {
        auto t1 = high_resolution_clock::now();
        fn();
        auto t2 = high_resolution_clock::now();
        fn();
        fn();
        auto t3 = high_resolution_clock::now();

        auto m1 = duration_cast<duration<double>>(t2 - t1).count();
        auto m2 = duration_cast<duration<double>>(t3 - t2).count();
        if (m2 > m1)
        {
            total += m2 - m1;
        }
        else
        {
            ++loops;
        } 
    }
    total /= loops;
    return total;
}

static bool setup = []
{
    DWORD mask = 1;
    SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)&mask);
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    return true;
}();

template < typename Fn > double measure(Fn&& fn)
{
    size_t loops = Iters;
    return measure(loops, std::forward< Fn >(fn));
}

template < typename Container, typename TestData > void insertion_test(Container& c, const TestData& data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(data[i]);
    }
}

template < typename Container, typename TestData > void insertion_test_hint(Container& c, const TestData& data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(c.end(), data[i]);
    }
}

template< typename T > const char* get_type_name() { return typeid(T).name(); }
template<> const char* get_type_name<std::string>() { return "std::string"; }

void print_results(const std::map< int, double >& results, const std::map< int, double >& base)
{
    for (auto& [i, t] : results)
    {
        std::cout << "\t" << i;
    }
    std::cout << std::endl;
    for (auto& [i, t] : results)
    {
        std::cout << "\t" << std::setprecision(2) << t/base.at(i);
    }
    std::cout << std::endl;
}

template < typename T > std::vector< T > get_vector_data(size_t count)
{
    std::vector< T > data(count);
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = value<T>(~i);
    }

    return data;
}

typedef boost::mpl::list<uint32_t> btree_perf_insert_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_insert, T, btree_perf_insert_types)
{
    std::map< int, double > base; 
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure([&]
        {
            std::set< T > c;
            insertion_test(c, data, i);
        });
    }
    
    {
        std::cout << "btree::set " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                btree::set< T > c;
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }
    
    {
        std::cout<< "boost::container::flat_set " << get_type_name<T>() << " insertion " << std::endl;
        std::map<int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                boost::container::flat_set< T > c;
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }
}

/*
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_insert_hint, T, btree_perf_insert_types)
{
    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure([&]
        {
            std::set< T > c;
            insertion_test(c, data, i);
        });
    }

    {
        std::cout << "btree::set " << get_type_name<T>() << " insertion with hint" << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                btree::set< T > c;
                insertion_test_hint(c, data, i);
            });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::container::flat_set " << get_type_name<T>() << " insertion with hint" << std::endl;
        std::map<int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                boost::container::flat_set< T > c;
                insertion_test_hint(c, data, i);
            });
        }
        print_results(results, base);
    }
}
*/

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_insert_arena, T, btree_perf_insert_types)
{
    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure([&]
        {
            crdt::arena< ArenaSize > arena;
            crdt::arena_allocator< T > arenaallocator(arena);
            std::set< T, std::less< T >, decltype(arenaallocator) > c(arenaallocator);
            insertion_test(c, data, i);
        });
    }

    {
        std::cout << "btree::set (arena) " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                crdt::arena< ArenaSize > arena;
                crdt::arena_allocator< void > arenaallocator(arena);
                btree::set< T, std::less< T >, decltype(arenaallocator) > c(arenaallocator);
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::container::flat_set (arena) " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double> results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                crdt::arena< ArenaSize > arena;
                crdt::arena_allocator< T > arenaallocator(arena);
                boost::container::flat_set< T, std::less< T >, decltype(arenaallocator) > c(arenaallocator);
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }
}

template < typename Container > void iteration_test(Container& c)
{
    volatile size_t x = 0;
    for (auto& v : c)
    {
        volatile auto p = &v;
        ++x;
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_iteration, T, btree_perf_insert_types)
{
    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        std::set< T > set(data.begin(), data.begin() + i);
        base[i] = measure([&] { iteration_test(set); });
    }

    {
        std::cout << "btree::set " << get_type_name<T>() << " iteration " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            btree::set< T > set;
            set.insert(data.begin(), data.begin() + i);
            results[i] = measure([&] { iteration_test(set); });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::flat_set " << get_type_name<T>() << " iteration " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            boost::container::flat_set< T > set(data.begin(), data.begin() + i);
            results[i] = measure([&] { iteration_test(set); });
        }
        print_results(results, base);
    }
}

#endif