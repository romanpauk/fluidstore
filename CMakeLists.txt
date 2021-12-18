project(fluidstore)

cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
add_link_options("$<$<CXX_COMPILER_ID:MSVC>:LINKER:/DEBUG:FASTLINK>")
enable_testing()

add_library(fluidstore INTERFACE)
target_include_directories(fluidstore INTERFACE include)

target_sources(fluidstore INTERFACE
    include/fluidstore/allocators/arena_allocator.h
    include/fluidstore/btree/detail/container.h
    include/fluidstore/btree/detail/fixed_vector.h
    include/fluidstore/btree/map.h
    include/fluidstore/btree/set.h
    include/fluidstore/crdts/allocator.h
    include/fluidstore/crdts/allocator_ptr.h
    include/fluidstore/crdts/dot.h
    include/fluidstore/crdts/dot_context.h
    include/fluidstore/crdts/dot_counters_base.h
    include/fluidstore/crdts/dot_kernel.h
    include/fluidstore/crdts/hook_default.h
    include/fluidstore/crdts/hook_extract.h
    include/fluidstore/crdts/set.h
    include/fluidstore/crdts/map.h
    include/fluidstore/crdts/noncopyable.h
    include/fluidstore/crdts/replica.h
    include/fluidstore/crdts/traits.h
    include/fluidstore/crdts/value_mv.h    
    include/fluidstore/flat/map.h
    include/fluidstore/flat/memory.h
    include/fluidstore/flat/set.h
    include/fluidstore/flat/vector.h
)

macro(group_sources path)
    file(GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${path} ${PROJECT_SOURCE_DIR}/${path}/*)
    foreach(child ${children})
        if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${path}/${child})
            group_sources(${path}/${child})
        else()
            string(REPLACE "/" "\\" groupname ${path})
            source_group(${groupname} FILES ${PROJECT_SOURCE_DIR}/${path}/${child})
        endif()
    endforeach()
endmacro()

find_package(Boost)
if(Boost_FOUND)
    add_executable(fluidstoretest 
        src/tests/btree.cpp
        src/tests/dot.cpp
        src/tests/flat.cpp
        src/tests/map.cpp
        src/tests/performance.cpp
        src/tests/set.cpp
        src/tests/testmodule.cpp
        src/tests/value_mv.cpp
        src/tests/pch.h
    )
    add_test(fluidstoretest COMMAND fluidstoretest)
    target_link_libraries(fluidstoretest fluidstore Boost::boost)
    target_precompile_headers(fluidstoretest PRIVATE src/tests/pch.h)

    target_compile_definitions(fluidstoretest PRIVATE DOTKERNEL_BTREE DOTCONTEXT_BTREE DOTCOUNTERS_BTREE)
    
    add_executable(fluidstoreperf 
        src/tests/perf.cpp
        src/tests/testmodule.cpp
    )
    add_test(fluidstoreperf COMMAND fluidstoreperf)
    target_link_libraries(fluidstoreperf fluidstore Boost::boost)
    target_precompile_headers(fluidstoreperf PRIVATE src/tests/pch.h)

    if(WIN32)
        group_sources(src)
        group_sources(include)
    endif()
endif()