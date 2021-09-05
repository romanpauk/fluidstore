#include <fluidstore/btree/btree.h>
#include <fluidstore/flat/set.h>

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

//_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);
//_CrtSetBreakAlloc(6668782);

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

template < typename T > T value(size_t);

template <> uint16_t value<uint16_t>(size_t i) { return i; }
template <> uint32_t value<uint32_t>(size_t i) { return i; }
template <> uint64_t value<uint64_t>(size_t i) { return i; }
template <> std::string value<std::string>(size_t i) { return std::to_string(i); }

typedef boost::mpl::list<uint16_t, uint32_t, uint64_t, std::string > test_types;

BOOST_AUTO_TEST_CASE(btree_node_capacity)
{
    // Note: number of elements is N * 2

    typedef btree::set< uint16_t > set_uint16_t;
    BOOST_TEST(set_uint16_t::N == 16);

    typedef btree::set< uint32_t > set_uint32_t;
    BOOST_TEST(set_uint32_t::N == 8);

    typedef btree::set< uint64_t > set_uint64_t;
    BOOST_TEST(set_uint64_t::N == 4);

    typedef btree::set< std::string > set_string;
    BOOST_TEST(set_string::N == 4);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_insert, T, test_types)
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

typedef boost::mpl::list<size_t> btree_range_for_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(btree_range_for, T, btree_range_for_types)
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_insert_loop, T, test_types)
{
    btree::set< T > c;
    for (int i = 0; i < 1000; ++i)
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

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_erase_loop, T, test_types)
{
//#if defined(_DEBUG)
    const int N = 100;
//#else
//    const int N = 1000;
//#endif

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

const int Loops = 10000;
const int Elements = 2;

template < typename Fn > double measure(size_t loops, Fn fn)
{
    using namespace std::chrono;

    auto begin = high_resolution_clock::now();
    for (size_t i = 0; i < loops; ++i)
    {
        fn();
    }
    auto end = high_resolution_clock::now();
    return duration_cast<duration<double>>(end - begin).count();
}

template < typename Container > void insertion_test(Container& c, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(value< typename Container::value_type >(i));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_insert, T, test_types)
{
    auto t1 = measure(Loops, [&] { std::set< T > c; insertion_test(c, Elements); });
    //std::cerr << "std::set " << typeid(T).name() << " insertion " << t1 << std::endl;

    auto t2 = measure(Loops, [&] { btree::set< T > c; insertion_test(c, Elements); });
    std::cerr << "btree::set " << typeid(T).name() << " insertion " << t2/t1 << std::endl;

    auto t3 = measure(Loops, [&] { std::allocator< T > allocator;  crdt::flat::set< T, std::allocator< T > > c(allocator); insertion_test(c, Elements); });
    std::cerr << "flat::set " << typeid(T).name() << " insertion " << t3 / t1 << std::endl;
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

template < typename Container > void fill_container(Container& c)
{
    for (size_t i = 0; i < Elements; ++i)
    {
        c.insert(value< typename Container::value_type >(i));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_iteration, T, test_types)
{
    double t1 = 0;
    {
        std::set< T > set;
        fill_container(set);
        t1 = measure(Loops, [&] { iteration_test(set); });
        //std::cerr << "std::set " << typeid(T).name() << " iteration " << t1 << std::endl;
    }

    {
        btree::set< T > set;
        fill_container(set);

        auto t2 = measure(Loops, [&] { iteration_test(set); });
        std::cerr << "btree::set " << typeid(T).name() << " iteration " << t2/t1 << std::endl;
    }

    {
        std::allocator< T > allocator;
        crdt::flat::set< T, std::allocator< T > > set(allocator);
        fill_container(set);

        auto t2 = measure(Loops, [&] { iteration_test(set); });
        std::cerr << "flat::set " << typeid(T).name() << " iteration " << t2 / t1 << std::endl;
    }
}

#endif