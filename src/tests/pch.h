#pragma once

#if defined(_WIN32)
    #define NOMINMAX

    // TODO: fix
    #define _ENFORCE_MATCHING_ALLOCATORS 0
#endif

#include <set>
#include <map>
#include <algorithm>
#include <iterator>
#include <cassert>
#include <scoped_allocator>
#include <chrono>
