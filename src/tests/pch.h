#pragma once

#if defined(_WIN32)
    #define NOMINMAX

    // TODO: fix
    #define _ENFORCE_MATCHING_ALLOCATORS 0
    #define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING 0 
    #define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING 0
    #define _CRT_SECURE_NO_WARNINGS

    #if defined(_DEBUG)
        #define _ITERATOR_DEBUG_LEVEL 1	// btree is not very conforming
        #define _CRTDBG_MAP_ALLOC
        #include <stdlib.h>
        #include <crtdbg.h>

        // Use _CrtSetBreakAlloc(x);
    #endif

    #include <windows.h>
    #include <profileapi.h>
#endif

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <scoped_allocator>
#include <memory>
#include <chrono>
#include <tuple>
#include <functional>
#include <iomanip>

#include <boost/mpl/list.hpp>
#include <boost/lexical_cast.hpp>


