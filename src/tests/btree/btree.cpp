#include <fluidstore/btree/map.h>
#include <fluidstore/btree/set.h>
#include <fluidstore/flat/set.h>
#include <fluidstore/memory/buffer_allocator.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
#include <iomanip>

#include "bench.h"

#if !defined(_DEBUG)
static int Iters = 20;
static int Max = 32768;
static const int ArenaSize = 65536 * 2;
#endif

static const int Count = 10;

template < typename T, size_t N > struct descriptor
{
    using size_type = size_t;

    descriptor()
        : size_()
    {}

    size_type size() const { return size_; }
    constexpr size_type capacity() const { return N; }

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
    btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > > c((descriptor < std::string, 8 >()));
    c.emplace_back(a, "2");
    c.emplace_back(a, "3");
    c.emplace(a, c.begin(), "1");
    c.clear(a);
}


BOOST_AUTO_TEST_CASE(btree_fixed_split_vector)
{
    std::allocator< char > a;
    
    btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > > v1((descriptor < std::string, 8 >()));
    btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > > v2((descriptor < std::string, 8 >()));

    btree::detail::fixed_split_vector < 
        btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > >,
        btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > >
    > c(std::move(v1), std::move(v2));

    btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > > v3((descriptor < std::string, 8 >()));
    btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > > v4((descriptor < std::string, 8 >()));

    btree::detail::fixed_split_vector <
        btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > >,
        btree::detail::fixed_vector < std::string, descriptor < std::string, 8 > >
    > c2(std::move(v3), std::move(v4));

    c.size();
    c.clear(a);
    c.begin();
    //c.erase(a, c.begin());
    c.emplace_back(a, std::make_tuple(std::string("a"), std::string("b")));
    c2.emplace_back(a, std::make_tuple("c", "d"));

    //c.insert(a, c.begin(), c2.begin(), c2.end());
    //auto y = c[0];

    c.clear(a);
    c2.clear(a);

    c2 = std::move(c);
}

template < typename T > static T value(size_t);

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
    BOOST_TEST(set_uint8_t::value_node::N == 32);

    typedef btree::set< uint16_t > set_uint16_t;
    BOOST_TEST(set_uint16_t::value_node::N == 16);

    typedef btree::set< uint32_t > set_uint32_t;
    BOOST_TEST(set_uint32_t::value_node::N == 8);

    typedef btree::set< uint64_t > set_uint64_t;
    BOOST_TEST(set_uint64_t::value_node::N == 4);

    typedef btree::set< std::string > set_string;
    BOOST_TEST(set_string::value_node::N == 4);
}

BOOST_AUTO_TEST_CASE(btree_map_node_dimension)
{
    // Note: Total number of elements in node is N * 2

    typedef btree::map< uint8_t, int > map_uint8_t;
    BOOST_TEST(map_uint8_t::value_node::N == 32);

    typedef btree::map< uint16_t, int > map_uint16_t;
    BOOST_TEST(map_uint16_t::value_node::N == 16);

    typedef btree::map< uint32_t, int > map_uint32_t;
    BOOST_TEST(map_uint32_t::value_node::N == 8);

    typedef btree::map< uint64_t, int > map_uint64_t;
    BOOST_TEST(map_uint64_t::value_node::N == 4);

    typedef btree::map< std::string, int > map_string;
    BOOST_TEST(map_string::value_node::N == 4);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_move, T, test_types)
{
 //   btree::set< T > set1;
 //   set1.insert(value<T>(1));

 //   btree::set< T > set2 = std::move(set1);
 //   BOOST_TEST(set1.size() == 0);
 //   BOOST_TEST(set2.size() == 1);
 //   BOOST_TEST((*set2.begin()) == value<T>(1));
}

BOOST_AUTO_TEST_CASE(btree_set_iterator)
{
    // This mostly tests that the code compiles, as there is some complexity in map<> iterator's ->.
    btree::set < std::string > set1;
    set1.insert("1");
    auto it = set1.begin();
    BOOST_TEST(it->size() == 1);
}

BOOST_AUTO_TEST_CASE(btree_map_iterator)
{
    // This mostly tests that the code compiles, as there is some complexity in map<> iterator's ->.
    btree::map < int, std::string > map1;
    map1.insert({ 1, "1" });
    auto it = map1.begin();
    BOOST_TEST(it->first == 1);
    BOOST_TEST(it->second == "1");
}

BOOST_AUTO_TEST_CASE(btree_set_erase)
{
    btree::set < int > set1;
    set1.insert(1);
    BOOST_TEST((set1.erase(set1.begin()) == set1.end()));
    BOOST_TEST(set1.size() == 0);
    BOOST_TEST(set1.empty());
    BOOST_TEST((set1.begin() == set1.end()));
}

BOOST_AUTO_TEST_CASE(btree_set_erase_iterator_forward)
{
    btree::set < int > set1;
    
    int Count = 512;
    for (int i = 0; i < Count; ++i)
    {
        set1.insert(i);
    }

    int i = 0;
    auto it = set1.begin();
    while (it != set1.end())
    {
        BOOST_TEST(*it == i++);
        it = set1.erase(it);
    }

    BOOST_TEST(i == Count);
}

BOOST_AUTO_TEST_CASE(btree_set_erase_iterator_backwards)
{
    btree::set < int > set1;

    int Count = 512;
    for (int i = 0; i < Count; ++i)
    {
        set1.insert(i);
    }

    int i = Count;
    while (!set1.empty())
    {
        auto it = --set1.end();
        BOOST_TEST(*it == --i);
        it = set1.erase(it);
        BOOST_TEST((it == set1.end()));
    }

    BOOST_TEST(i == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_move, T, test_types)
{
    btree::map< T, T > map1;
    map1.insert({ value<T>(1), value<T>(1) });

    btree::map< T, T > map2 = std::move(map1);
    BOOST_TEST(map1.size() == 0);
    BOOST_TEST(map2.size() == 1);
    BOOST_TEST((*map2.begin()).first == value<T>(1));
    BOOST_TEST((*map2.begin()).second == value<T>(1));
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

BOOST_AUTO_TEST_CASE(btree_set_insert_end_hint)
{
    btree::set< int > c;
    for (int i = 0; i < 1000; ++i)
    {
        c.insert(c.end(), i);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_insert, T, test_types)
{
    btree::map< T, T > c;
    BOOST_TEST(c.size() == 0);
    BOOST_TEST(c.empty());
    BOOST_TEST((c.find(value<T>(0)) == c.end()));

    {
        auto pairb = c.insert({ value<T>(2), value<T>(2) });
        BOOST_TEST(pairb.second == true);
        // TODO: does not compile
        //BOOST_TEST((*pairb.first == std::make_pair(value<T>(2), value<T>(2))));
        BOOST_TEST((c.find(value<T>(2)) == pairb.first));
        BOOST_TEST((c.find(value<T>(2)) != c.end()));

        BOOST_TEST(c.size() == 1);
        BOOST_TEST(c.insert({ value<T>(2), value<T>(2) }).second == false);
        BOOST_TEST(c.size() == 1);
    }

    c.insert({ value<T>(1), value<T>(1) });
    BOOST_TEST(c.size() == 2);
    BOOST_TEST((c.find(value<T>(1)) != c.end()));
}

typedef boost::mpl::list<size_t> btree_range_for_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_range_for, T, btree_range_for_types)
{
    for (int i = 0; i < Count; ++i)
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_range_for, T, btree_range_for_types)
{
    for (int i = 0; i < Count; ++i)
    {
        btree::map< T, T > c;

        for (int j = 0; j < i; ++j)
        {
            c.insert({ value<T>(j), value<T>(j) });
        }

        int k = 0;
        for (auto&& v : c)
        {
            BOOST_REQUIRE(v.first == value<T>(k));
            BOOST_REQUIRE(v.second == value<T>(k++));

            //BOOST_REQUIRE(v == value<T>(k++));
        }

        BOOST_TEST(k == c.size());
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_insert_loop, T, test_types)
{
    if ((1ull << sizeof(T)) < Count)
    {
        return;
    }

    btree::set< T > c;
    for (int i = 0; i < Count; ++i)
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_insert_loop, T, test_types)
{
    if ((1ull << sizeof(T)) < Count)
    {
        return;
    }

    btree::map< T, T > c;
    for (int i = 0; i < Count; ++i)
    {
        auto iv = value<T>(i);
        BOOST_REQUIRE(c.insert({ iv,iv }).second);
        BOOST_REQUIRE((c.find(iv) != c.end()));
        BOOST_REQUIRE((*c.find(iv)).first == iv);

        // Check that the tree was not damaged by insertion
        for (int j = 0; j < i; ++j)
        {
            auto jv = value<T>(j);
            BOOST_REQUIRE((c.find(jv) != c.end()));
            BOOST_REQUIRE((*c.find(jv)).first == jv);
            BOOST_REQUIRE((*c.find(jv)).second == jv);
        }
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_erase_loop, T, test_types)
{
    if ((1ull << sizeof(T)) < Count)
    {
        return;
    }

    for (int i = 0; i < Count; ++i)
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

    for (int i = 0; i < Count; ++i)
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_erase_loop, T, test_types)
{
    if ((1ull << sizeof(T)) < Count)
    {
        return;
    }

    for (int i = 0; i < Count; ++i)
    {
        btree::map< T, T > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert({ value<T>(j), value<T>(j) });
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

    for (int i = 0; i < Count; ++i)
    {
        btree::map< T, T > c;
        for (int j = 0; j < i; ++j)
        {
            c.insert({ value<T>(j), value<T>(j) });
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

BOOST_AUTO_TEST_CASE(btree_lower_bound)
{
    btree::set< int > c;
    BOOST_TEST((c.lower_bound(0) == c.end()));
    c.insert(1);
    BOOST_TEST((c.lower_bound(0) == c.find(1)));
    BOOST_TEST((c.lower_bound(1) == c.find(1)));
    c.insert(2);
    BOOST_TEST((c.lower_bound(2) == c.find(2)));
}

BOOST_AUTO_TEST_CASE(btree_lower_bound_loop)
{
    btree::set< int > c;
    for (int i = 2; i < 1000; i += 2)
    {
        c.insert(i);
        BOOST_TEST((c.lower_bound(i - 1) == c.find(i)));
    }
}

BOOST_AUTO_TEST_CASE(btree_lower_bound_pair)
{
    btree::set< std::pair< int, int > > counters;

    counters.insert({ 0, 1 });
    counters.insert({ 1, 1 });

    BOOST_TEST((counters.lower_bound({ 0, 0 }) == counters.find({ 0, 1 })));
    BOOST_TEST((counters.lower_bound({ 0, 1 }) == counters.find({ 0, 1 })));
    BOOST_TEST((counters.lower_bound({ 0, 2 }) == counters.find({ 1, 1 })));
    BOOST_TEST((counters.lower_bound({ 0, std::numeric_limits< int >::max() }) == counters.find({ 1, 1 })));
    BOOST_TEST((counters.lower_bound({ 1, 0 }) == counters.find({ 1, 1 })));
    BOOST_TEST((counters.lower_bound({ 1, 1 }) == counters.find({ 1, 1 })));
    BOOST_TEST((counters.lower_bound({ 1, 2 }) == counters.end()));
}

#if !defined(_DEBUG)
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

template< typename T > static const char* get_type_name() { return typeid(T).name(); }
template<> const char* get_type_name<std::string>() { return "std::string"; }

template < typename T > T tr(T value)
{
    return ~value;
}

template < typename T > std::vector< T > get_vector_data(size_t count)
{
    std::vector< T > data(count);
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = value<T>(tr(i));
    }

    return data;
}

template < typename T > std::vector< std::pair< T, T > > get_vector_data_pair(size_t count)
{
    std::vector< std::pair< T, T > > data(count);
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = { value<T>(tr(i)), value<T>(tr(i)) };
    }

    return data;
}

typedef boost::mpl::list < uint32_t > btree_perf_insert_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_perf_insert, T, btree_perf_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base; 
    auto data = get_vector_data< T >(Max);
    std::sort(data.begin(), data.end());

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

typedef boost::mpl::list < uint32_t > btree_perf_insert_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_perf_insert_hint, T, btree_perf_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);
    std::sort(data.begin(), data.end());

    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure([&]
            {                
                std::set< T > c;      
                //btree::set< T > c;
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_perf_insert, T, btree_perf_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data_pair< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure([&]
        {
            std::map< T, T > c;
            insertion_test(c, data, i);
        });
    }

    {
        std::cout << "btree::map " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                btree::map< T, T > c;
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::container::flat_map " << get_type_name<T>() << " insertion " << std::endl;
        std::map<int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure([&]
            {
                boost::container::flat_map< T, T > c;
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_perf_insert_arena, T, btree_perf_insert_types)
{
    const int preallocated = 1 << 24;

    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {    
        memory::dynamic_buffer<> buffer(preallocated);
        memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

        base[i] = measure([&, allocator]
        {            
            std::set< T, std::less< T >, decltype(allocator) > c(allocator);
            insertion_test(c, data, i);
        });
    }

    {
        std::cout << "btree::set (arena) " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer<> buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

            results[i] = measure([&, allocator]
            {
                btree::set< T, std::less< T >, decltype(allocator) > c(allocator);
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
            memory::dynamic_buffer<> buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

            results[i] = measure([&, allocator]
            {
                boost::container::flat_set< T, std::less< T >, decltype(allocator) > c(allocator);
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_perf_insert_arena, T, btree_perf_insert_types)
{
    const int preallocated = 1 << 24;

    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data_pair< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        memory::dynamic_buffer<> buffer(preallocated);
        memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

        base[i] = measure([&, allocator]
        {
            std::map< T, T, std::less< T >, decltype(allocator) > c(allocator);
            insertion_test(c, data, i);
        });
    }

    {
        std::cout << "btree::map (arena) " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer<> buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

            results[i] = measure([&, allocator]
            {
                btree::map< T, T, std::less< T >, decltype(allocator) > c(allocator);
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::container::flat_map (arena) " << get_type_name<T>() << " insertion " << std::endl;
        std::map< int, double> results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer<> buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > allocator(buffer);

            results[i] = measure([&, allocator]
            {
                boost::container::flat_map< T, T, std::less< T >, decltype(allocator) > c(allocator);
                insertion_test(c, data, i);
            });
        }
        print_results(results, base);
    }
}

template < typename Container > void iteration_test(Container& c)
{
    volatile size_t x = 0;
    for (auto&& v : c)
    {
        (void)v;
        ++x;
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_set_perf_iteration, T, btree_perf_insert_types)
{
    set_realtime_priority();

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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_perf_iteration, T, btree_perf_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data_pair< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        std::map< T, T > set(data.begin(), data.begin() + i);
        base[i] = measure([&] { iteration_test(set); });
    }

    {
        std::cout << "btree::map " << get_type_name<T>() << " iteration " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            btree::map< T, T > set;
            set.insert(data.begin(), data.begin() + i);
            results[i] = measure([&] { iteration_test(set); });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::flat_map " << get_type_name<T>() << " iteration " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            boost::container::flat_map< T, T > set(data.begin(), data.begin() + i);
            results[i] = measure([&] { iteration_test(set); });
        }
        print_results(results, base);
    }
}

template < typename Container, typename TestData > void find_test(Container& c, const TestData& data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        volatile bool x = c.find(data[i]) == c.end();
        (x);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_map_set_find, T, btree_perf_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);

    for (size_t i = 1; i < Max; i *= 2)
    {
        std::set< T > set(data.begin(), data.begin() + i);
        base[i] = measure([&] { find_test(set, data, i); });
    }

    {
        std::cout << "btree::set " << get_type_name<T>() << " find " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            btree::set< T > set;
            set.insert(data.begin(), data.begin() + i);
            results[i] = measure([&] { find_test(set, data, i); });
        }
        print_results(results, base);
    }

    {
        std::cout << "boost::flat_set " << get_type_name<T>() << " find " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            boost::container::flat_set< T > set(data.begin(), data.begin() + i);
            results[i] = measure([&] { find_test(set, data, i); });
        }
        print_results(results, base);
    }
}

BOOST_AUTO_TEST_CASE(btree_memory_overhead)
{
    const int Max = 32768*4;
    std::map< size_t, double > base;
    
    for (int i = 1; i < Max; i *= 2)
    {
        memory::stats_buffer<> buffer;
        memory::buffer_allocator< int, decltype(buffer) > allocator(buffer);

        std::set< int, std::less< int >,  decltype(allocator) > s(allocator);
        for (int j = 0; j < i; ++j)
        {
            s.insert(j);
        }

        base[i] = buffer.get_allocated();
    }

    std::map< size_t, double > results;
    for (int i = 1; i < Max; i *= 2)
    {
        memory::stats_buffer<> buffer;
        memory::buffer_allocator< int, decltype(buffer) > allocator(buffer);

        btree::set< int, std::less< int >, decltype(allocator) > s(allocator);
        for (int j = 0; j < i; ++j)
        {
            s.insert(j);
        }

        results[i] = buffer.get_allocated();
    }

    std::cout << "btree memory usage compared to std::set" << std::endl;
    print_results(results, base);
}

#endif