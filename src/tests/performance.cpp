#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/set.h>
#include <fluidstore/crdts/hook_extract.h>

#include <boost/test/unit_test.hpp>

template < typename Fn > double measure(Fn fn)
{
    using namespace std::chrono;

    auto begin = high_resolution_clock::now();
    fn();
    auto end = high_resolution_clock::now();
    return duration_cast<duration<double>>(end - begin).count();
}

#if !defined(_DEBUG)

template < typename T > T tr(T val) 
{
    //return val;
    return ~(val << 13); 
}

BOOST_AUTO_TEST_CASE(set_insert_performance)
{
#define Outer 10000
#define Inner 1000
//#define PROFILE

#if !defined(PROFILE)
    auto t1 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            std::set< size_t > set;
            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(tr(i));
            }
        }
    });
    std::cerr << "std::set " << t1 << std::endl;

    auto t2 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            crdt::id_sequence<> sequence;
            crdt::replica<> replica(0, sequence);
            crdt::allocator< crdt::replica<> > allocator(replica);
            crdt::set2< size_t, decltype(allocator), crdt::tag_state /*, crdt::hook_extract*/ > set(allocator);

            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(tr(i));
            }
        }
    });
    
    std::cerr << "crdt::set " << t2 << " (normal) slowdown " << t2 / t1 << std::endl;
#endif
    auto t3 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            crdt::id_sequence<> sequence;
            crdt::replica<> replica(0, sequence);
            crdt::arena< 32768 > arena;
            crdt::arena_allocator< void > arenaallocator(arena);
            crdt::allocator< crdt::replica<>, void, crdt::arena_allocator< void > > allocator(replica, arenaallocator);

            crdt::set2 < size_t, decltype(allocator), crdt::tag_state /*, crdt::hook_extract*/ > set(allocator);

            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(tr(i));
            }
        }
    });

#if !defined(PROFILE)
    std::cerr << "crdt::set " << t3 << " (arena) slowdown " << t3 / t1 << std::endl;
#endif
}

#endif
