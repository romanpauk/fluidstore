#pragma once

#if defined(_WIN32)
    #define NOMINMAX

    // TODO: fix
    #define _ENFORCE_MATCHING_ALLOCATORS 0
    #define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING 0 
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

#include <boost/mpl/list.hpp>
#include <boost/lexical_cast.hpp>


