#include <fluidstore/btree/detail/fixed_vector.h>

#include <boost/test/unit_test.hpp>

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

BOOST_AUTO_TEST_CASE(is_fixed_vector_trivial)
{
    BOOST_TEST(btree::detail::is_fixed_vector_trivial< int >::value == true);
    BOOST_TEST(btree::detail::is_fixed_vector_trivial< int* >::value == true);
    BOOST_TEST(btree::detail::is_fixed_vector_trivial< std::string >::value == false);
    BOOST_TEST((btree::detail::is_fixed_vector_trivial< std::pair< int, int > >::value == true));
    BOOST_TEST((btree::detail::is_fixed_vector_trivial< std::pair< int&, int& > >::value == true));
}

BOOST_AUTO_TEST_CASE(fixed_vector_basic_operations)
{
    std::allocator< uint8_t > alloc;
    btree::detail::fixed_vector < int, descriptor < int, 8 > > vec((descriptor < int, 8 >()));

    //
    BOOST_TEST(vec.empty());

    // 1
    vec.emplace_back(alloc, 1);
    BOOST_TEST(vec.size() == 1);
    BOOST_TEST(!vec.empty());
    BOOST_TEST(vec[0] == 1);

    // 1 2
    vec.emplace_back(alloc, 2);
    BOOST_TEST(vec.size() == 2);
    BOOST_TEST(vec[1] == 2);

    // 1 2 3
    vec.emplace(alloc, vec.end(), 3);
    BOOST_TEST(vec.size() == 3);
    BOOST_TEST(vec[2] == 3);

    // 4 1 2 3
    vec.emplace(alloc, vec.begin(), 4);
    BOOST_TEST(vec.size() == 4);
    BOOST_TEST(vec[0] == 4);
    BOOST_TEST(vec[1] == 1);
    BOOST_TEST(vec[2] == 2);
    BOOST_TEST(vec[3] == 3);

    // 4 5 1 2 3
    vec.emplace(alloc, vec.begin() + 1, 5);
    BOOST_TEST(vec.size() == 5);
    BOOST_TEST(vec[0] == 4);
    BOOST_TEST(vec[1] == 5);
    BOOST_TEST(vec[2] == 1);
    // ...

    // 4 5
    vec.erase(alloc, vec.begin() + 2, vec.end());
    BOOST_TEST(vec.size() == 2);
    BOOST_TEST(vec[0] == 4);
    BOOST_TEST(vec[1] == 5);

    // 5
    vec.erase(alloc, vec.begin());
    BOOST_TEST(vec.size() == 1);
    BOOST_TEST(vec[0] == 5);

    // 5 6 7 8
    vec.emplace(alloc, vec.end(), 8);
    int vals[] = { 6, 7 };
    vec.insert(alloc, vec.begin() + 1, vals, vals + 2);
    BOOST_TEST(vec.size() == 4);
    BOOST_TEST(vec[0] == 5);
    BOOST_TEST(vec[1] == 6);
    BOOST_TEST(vec[2] == 7);
    BOOST_TEST(vec[3] == 8);

    BOOST_TEST(vec.front() == 5);
    BOOST_TEST(vec.back() == 8);

    vec.erase(alloc, vec.begin(), vec.end());
    BOOST_TEST(vec.size() == 0);
    BOOST_TEST(vec.empty());
}

BOOST_AUTO_TEST_CASE(fixed_vector_iteration)
{
    std::allocator< uint8_t > alloc;
    btree::detail::fixed_vector < int, descriptor < int, 8 > > vec((descriptor < int, 8 >()));

    for (int i = 0; i < vec.capacity(); ++i)
    {
        vec.emplace_back(alloc, i);
    }

    BOOST_TEST(vec.size() == 8);

    auto it = vec.begin();
    for (int i = 0; i < vec.capacity(); ++i)
    {
        BOOST_TEST(vec[i] == *it++);
    }

    vec.clear(alloc);
    BOOST_TEST(vec.size() == 0);
}
