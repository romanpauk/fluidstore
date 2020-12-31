#include <fluidstore/allocators/arena_allocator.h>
#include <fluidstore/crdts/allocator.h>
#include <fluidstore/crdts/set.h>

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

BOOST_AUTO_TEST_CASE(set_insert_performance)
{
    /*
#define Outer 10000
#define Inner 100

    auto t1 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            std::set< size_t > set;
            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });
    
    auto t2 = measure([]
    {
        for (size_t x = 0; x < Outer; ++x)
        {
            crdt::id_sequence<> sequence;
            crdt::replica<> replica(0, sequence);
            crdt::allocator< crdt::replica<> > allocator(replica);
            crdt::set< size_t, decltype(allocator) > set(allocator);

            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });
    
    auto t3 = measure([]
    {
        crdt::arena< 32768 > arena;
        crdt::arena< 32768 > arena2;

        for (size_t x = 0; x < Outer; ++x)
        {
            crdt::id_sequence<> sequence;
            crdt::replica<> replica(0, sequence);
            crdt::tagged_type< crdt::tag_state, crdt::allocator<> > a1(replica);
            //crdt::tagged_type< crdt::tag_state, crdt::arena_allocator< void, crdt::allocator<> > > a1(arena2, replica);
            crdt::tagged_type< crdt::tag_delta, crdt::arena_allocator< void, crdt::allocator<> > > a2(arena, replica);
            crdt::tagged_allocator< crdt::replica<>, decltype(a1), decltype(a2) > allocator(replica, a1, a2);

            crdt::set< size_t, decltype(allocator) > set(allocator);

            for (size_t i = 0; i < Inner; ++i)
            {
                set.insert(i);
            }
        }
    });

    std::cerr << "std::set " << t1 << std::endl;
    std::cerr << "crdt::set " << t2 << " (normal) slowdown " << t2 / t1 << std::endl;
    std::cerr << "crdt::set " << t3 << " (optimi) slowdown " << t3 / t1 << std::endl;
    */
}

#endif