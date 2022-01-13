#include <fluidstore/crdt/allocator.h>
#include <fluidstore/crdt/set.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_btree.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_stl.h>
#include <fluidstore/crdt/detail/dot_kernel_metadata_stl.h>
#include <fluidstore/crdt/hooks/hook_extract.h>

#include <boost/test/unit_test.hpp>

#include <iomanip>

#include "bench.h"

static int Max = 32768;
static int Iters = 20;

#if !defined(_DEBUG)

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

typedef boost::mpl::list < uint32_t > crdt_set_insert_types;
BOOST_AUTO_TEST_CASE_TEMPLATE(crdt_set_insert, T, crdt_set_insert_types)
{
    set_realtime_priority();

    std::map< int, double > base;
    auto data = get_vector_data< T >(Max);
    
    for (size_t i = 1; i < Max; i *= 2)
    {
        base[i] = measure(Iters, [&]
            {
                std::set< T > c;
                insertion_test(c, data, i);
            });
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", btree>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure(Iters, [&]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<> > allocator(replica);
                    crdt::set< size_t, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_btree_local, crdt::hook_default/*, crdt::hook_extract*/ > c(allocator);
                                        
                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", stl>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure(Iters, [&]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<> > allocator(replica);
                    crdt::set< size_t, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_stl_local, crdt::hook_default/*, crdt::hook_extract*/ > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }

#if defined(_WIN32)
    {
        std::cout << "crdt::set<" << get_type_name<T>() << ", flat>" << " insertion " << std::endl;
        std::map< int, double > results;
        for (size_t i = 1; i < Max; i *= 2)
        {
            results[i] = measure(Iters, [&]
                {
                    crdt::replica<> replica(0);
                    crdt::allocator< crdt::replica<> > allocator(replica);
                    crdt::set< size_t, decltype(allocator), crdt::tag_state, crdt::detail::metadata_tag_flat_local, crdt::hook_default/*, crdt::hook_extract*/ > c(allocator);

                    insertion_test(c, data, i);
                });
        }
        print_results(results, base);
    }
#endif
}


#endif
