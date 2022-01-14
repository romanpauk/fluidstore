#pragma once

#if defined(_WIN32)
#include <windows.h>
#include <profileapi.h>
#endif


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

static void set_realtime_priority()
{
#if defined(_WIN32)
    DWORD mask = 1;
    SetProcessAffinityMask(GetCurrentProcess(), (DWORD_PTR)&mask);
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
#endif
}

template < typename Elements, typename Value > static void print_results(const std::map< Elements, Value >& results, const std::map< Elements, Value >& base)
{
    for (auto& [i, t] : results)
    {
        std::cout << "\t" << i;
    }
    std::cout << std::endl;
    for (auto& [i, t] : results)
    {
        std::cout << "\t" << std::setprecision(2) << t / base.at(i);
    }
    std::cout << std::endl;
}

