#include <fluidstore/crdt/allocator.h>
#include <fluidstore/crdt/set.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_btree.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_stl.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_stl.h>
#include <fluidstore/crdt/hooks/hook_extract.h>

#include <fluidstore/memory/buffer_allocator.h>

#include <boost/test/unit_test.hpp>

#include <iomanip>

#include "bench.h"
#include "shuffle.h"

#if defined(TLX_ENABLED)
#include <tlx/container/btree_set.hpp>
void tlx::die_with_message(char const*, char const*, unsigned __int64) {}
#endif

#if defined(ABSL_ENABLED)
#include <absl/container/btree_set.h>
#endif

#if !defined(_DEBUG)

static int Max = 8192;
static int Iters = 20;

template < typename T > static T value(size_t);

template <> uint8_t value<uint8_t>(size_t i) { return static_cast<uint8_t>(i); }
template <> uint16_t value<uint16_t>(size_t i) { return static_cast<uint16_t>(i); }
template <> uint32_t value<uint32_t>(size_t i) { return static_cast<uint32_t>(i);; }
template <> uint64_t value<uint64_t>(size_t i) { return static_cast<uint64_t>(i);; }
template <> std::string value<std::string>(size_t i) { return std::to_string(i); }

template < typename T > std::vector< T > get_vector_data(size_t count)
{
    std::vector< T > data(count);
    for (size_t i = 0; i < count; ++i)
    {
        data[i] = ~value<T>(i);
    }

    return data;
}

template< typename T > const char* get_type_name() { return typeid(T).name(); }
template<> const char* get_type_name<std::string>() { return "std::string"; }

template < typename Container, typename TestData > void insertion_test(Container& c, const TestData& data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(data[i]);
    }
}

template < typename Container, typename TestData > void insertion_test_end(Container& c, const TestData& data, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        c.insert(c.end(), data[i]);
    }
}

typedef boost::mpl::list < uint32_t > crdt_set_insert_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(crdt_set_insert, T, crdt_set_insert_types)
{
    const int preallocated = 1 << 24;
    
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);
    std::sort(data.begin(), data.end());

    for (size_t i = 1; i < Max; i *= 2)
    {        
        memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
        memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);
                
        base[i] = measure(Iters, [&, alloc]
            {                
                std::set< T, std::less< T >, decltype(alloc) > c(alloc);
                insertion_test(c, data, i);
            });
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", btree, default>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_default > c(allocator);
                                        
                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", btree, extract>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_extract > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", stl, default>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_stl_local, crdt::hook_default > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", stl, extract>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_stl_local, crdt::hook_extract > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", flat, default>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_flat_local, crdt::hook_default > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", flat, extract>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            results[i] = measure(Iters, [&, alloc]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<>, T, decltype(alloc) > allocator(replica, alloc);
                    crdt::set< T, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_flat_local, crdt::hook_extract > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(set_insert, T, crdt_set_insert_types)
{
    std::cout.imbue(std::locale(""));

    const int N = 1000000;
    const int preallocated = 1 << 30;

    {
        auto data = get_vector_data< T >(N);
        shuffle(data);

        {
            auto t = measure(Iters, [&]
                {
                    std::set< T > c;
                    insertion_test(c, data, N);
                });

            std::cout << "std::set<" << get_type_name<T>() << ">" << " [random,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    std::set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test(c, data, N);
                });

            std::cout << "std::set<" << get_type_name<T>() << ">" << " [random,linear] insertions per second: " << int(N / t) << std::endl;
        }

        {
            auto t = measure(Iters, [&]
                {
                    btree::set< T > c;
                    insertion_test(c, data, N);
                });

            std::cout << "btree::set<" << get_type_name<T>() << ">" << " [random,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    btree::set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test(c, data, N);
                });

            std::cout << "btree::set<" << get_type_name<T>() << ">" << " [random,linear] insertions per second: " << int(N / t) << std::endl;
        }

    #if defined(TLX_ENABLED)
        {
            auto t = measure(Iters, [&]
                {
                    tlx::btree_set< T > c;
                    insertion_test(c, data, N);
                });

            std::cout << "tlx::btree_set<" << get_type_name<T>() << ">" << " [random,new] insertions per second: " << int(N / t) << std::endl;
        }
        
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    tlx::btree_set< T, std::less< T >, tlx::btree_default_traits<T, T>, decltype(alloc) > c(alloc);
                    insertion_test(c, data, N);
                });

            std::cout << "tlx::btree_set<" << get_type_name<T>() << ">" << " [random,linear] insertions per second: " << int(N / t) << std::endl;
        }
    #endif

    #if defined(ABSL_ENABLED)
        {
            auto t = measure(Iters, [&]
                {
                    absl::btree_set< T > c;
                    insertion_test(c, data, N);
                });

            std::cout << "absl::btree_set<" << get_type_name<T>() << ">" << " [random,new] insertions per second: " << int(N / t) << std::endl;
        }
        
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    absl::btree_set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test(c, data, N);
                });

            std::cout << "absl::btree_set<" << get_type_name<T>() << ">" << " [random,linear] insertions per second: " << int(N / t) << std::endl;
        }
    #endif
    }

    {
        auto data = get_vector_data< T >(N);
        std::sort(data.begin(), data.end());

        {
            auto t = measure(Iters, [&]
                {
                    std::set< T > c;
                    insertion_test_end(c, data, N);
                });

            std::cout << "std::set<" << get_type_name<T>() << ">" << " [append,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    std::set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test_end(c, data, N);
                });

            std::cout << "std::set<" << get_type_name<T>() << ">" << " [append,linear] insertions per second: " << int(N / t) << std::endl;
        }

        {
            auto t = measure(Iters, [&]
                {
                    btree::set< T > c;
                    insertion_test_end(c, data, N);
                });

            std::cout << "btree::set<" << get_type_name<T>() << ">" << " [append,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    btree::set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test_end(c, data, N);
                });

            std::cout << "btree::set<" << get_type_name<T>() << ">" << " [append,linear] insertions per second: " << int(N / t) << std::endl;
        }

        {
            auto t = measure(Iters, [&]
                {
                    std::deque< T > c;
                    for (size_t i = 0; i < N; ++i)
                    {
                        c.push_back(i);
                    }                    
                });

            std::cout << "std::deque<" << get_type_name<T>() << ">" << " [append,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    std::deque < T, decltype(alloc) > c(alloc);
                    for (size_t i = 0; i < N; ++i)
                    {
                        c.push_back(i);
                    }
                });

            std::cout << "std::deque<" << get_type_name<T>() << ">" << " [append,linear] insertions per second: " << int(N / t) << std::endl;
        }

    #if defined(TLX_ENABLED)
        {
            auto t = measure(Iters, [&]
                {
                    tlx::btree_set< T > c;
                    insertion_test_end(c, data, N);
                });

            std::cout << "tlx::btree_set<" << get_type_name<T>() << ">" << " [append,new] insertions per second: " << int(N / t) << std::endl;
        }

        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    tlx::btree_set< T, std::less< T >, tlx::btree_default_traits<T, T>, decltype(alloc) > c(alloc);
                    insertion_test_end(c, data, N);
                });

            std::cout << "tlx::btree_set<" << get_type_name<T>() << ">" << " [append,linear] insertions per second: " << int(N / t) << std::endl;
        }
    #endif

    #if defined(ABSL_ENABLED)
        {
            auto t = measure(Iters, [&]
                {
                    absl::btree_set< T > c;
                    insertion_test_end(c, data, N);
                });

            std::cout << "absl::btree_set<" << get_type_name<T>() << ">" << " [append,new] insertions per second: " << int(N / t) << std::endl;
        }
        
        {
            memory::dynamic_buffer< std::allocator< uint8_t > > buffer(preallocated);
            memory::buffer_allocator< T, decltype(buffer) > alloc(buffer);

            auto t = measure(Iters, [&, alloc]
                {
                    absl::btree_set< T, std::less< T >, decltype(alloc) > c(alloc);
                    insertion_test_end(c, data, N);
                });

            std::cout << "absl::btree_set<" << get_type_name<T>() << ">" << " [append,linear] insertions per second: " << int(N / t) << std::endl;
        }
    #endif
    }
}

BOOST_AUTO_TEST_CASE(crdt_set_memory_overhead)
{
    const int Max = 32768 * 4;
    std::map< size_t, double > base;

    for (int i = 1; i < Max; i *= 2)
    {
        memory::stats_buffer<> buffer;
        memory::buffer_allocator< int, decltype(buffer) > allocator(buffer);

        std::set< int, std::less< int >, decltype(allocator) > s(allocator);
        for (int j = 0; j < i; ++j)
        {
            s.insert(j);
        }

        base[i] = buffer.get_allocated();
    }

    {
        std::map< size_t, double > results;
        for (int i = 1; i < Max; i *= 2)
        {
            memory::stats_buffer<> buffer;
            memory::buffer_allocator< int, decltype(buffer) > bufferallocator(buffer);

            crdt::replica<> replica(0);
            crdt::allocator< crdt::replica<>, int, decltype(bufferallocator) > allocator(replica, bufferallocator);

            crdt::set< int, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_default > s(allocator);
            for (int j = 0; j < i; ++j)
            {
                s.insert(j);
            }

            results[i] = buffer.get_allocated();
        }

        std::cout << "crdt::set [btree] memory usage compared to std::set" << std::endl;
        print_results(results, base);
    }

    {
        std::map< size_t, double > results;
        for (int i = 1; i < Max; i *= 2)
        {
            memory::stats_buffer<> buffer;
            memory::buffer_allocator< int, decltype(buffer) > bufferallocator(buffer);

            crdt::replica<> replica(0);
            crdt::allocator< crdt::replica<>, int, decltype(bufferallocator) > allocator(replica, bufferallocator);

            crdt::set< int, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_stl_local, crdt::hook_default > s(allocator);
            for (int j = 0; j < i; ++j)
            {
                s.insert(j);
            }

            results[i] = buffer.get_allocated();
        }

        std::cout << "crdt::set [stl] memory usage compared to std::set" << std::endl;
        print_results(results, base);
    }

    {
        std::map< size_t, double > results;
        for (int i = 1; i < Max; i *= 2)
        {
            memory::stats_buffer<> buffer;
            memory::buffer_allocator< int, decltype(buffer) > bufferallocator(buffer);

            crdt::replica<> replica(0);
            crdt::allocator< crdt::replica<>, int, decltype(bufferallocator) > allocator(replica, bufferallocator);

            crdt::set< int, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_flat_local, crdt::hook_default > s(allocator);
            for (int j = 0; j < i; ++j)
            {
                s.insert(j);
            }

            results[i] = buffer.get_allocated();
        }

        std::cout << "crdt::set [flat] memory usage compared to std::set" << std::endl;
        print_results(results, base);
    }
}

#endif
