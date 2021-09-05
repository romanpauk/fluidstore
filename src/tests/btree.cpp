#include <fluidstore/btree/btree.h>

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

template <> uint32_t value<uint32_t>(size_t i) { return i; }
template <> uint64_t value<uint64_t>(size_t i) { return i; }
template <> std::string value<std::string>(size_t i) { return std::to_string(i); }

typedef boost::mpl::list<uint32_t, uint64_t, std::string > test_types;

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

const int Loops = 100000;
const int Elements = 8;

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

template < typename Container, typename T > void insertion_test(size_t count)
{
    Container c;
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(value<T>(i));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_insert, T, test_types)
{
    auto t1 = measure(Loops, [&] { insertion_test< std::set< T >, T >(Elements); });
    //std::cerr << "std::set " << typeid(T).name() << " insertion " << t1 << std::endl;

    auto t2 = measure(Loops, [&] { insertion_test< btree::set< T >, T >(Elements); });
    std::cerr << "btree::set " << typeid(T).name() << " insertion " << t2/t1 << std::endl;
}

template < typename T, typename Container > void iteration_test(Container& c)
{
    volatile size_t x = 0;
    for (auto& v : c)
    {
        volatile auto p = &v;
        ++x;
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(btree_perf_iteration, T, test_types)
{
    double t1 = 0;
    {
        std::set< T > set;
        for (size_t i = 0; i < Elements; ++i)
        {
            set.insert(value<T>(i));
        }

        t1 = measure(Loops, [&] { iteration_test<T>(set); });
        //std::cerr << "std::set " << typeid(T).name() << " iteration " << t1 << std::endl;
    }

    {
        btree::set< T > set;
        for (size_t i = 0; i < Elements; ++i)
        {
            set.insert(value<T>(i));
        }

        auto t2 = measure(Loops, [&] { iteration_test<T>(set); });
        std::cerr << "btree::set " << typeid(T).name() << " iteration " << t2/t1 << std::endl;
    }
}

#endif