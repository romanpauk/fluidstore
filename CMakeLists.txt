project(fluidstore)

cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
add_link_options("$<$<CXX_COMPILER_ID:MSVC>:LINKER:/DEBUG:FASTLINK>")
enable_testing()

add_library(fluidstore INTERFACE)
target_include_directories(fluidstore INTERFACE include)

target_sources(fluidstore INTERFACE
    include/fluidstore/crdts/allocator.h
    include/fluidstore/crdts/allocator_ptr.h
    include/fluidstore/crdts/allocator_traits.h
    include/fluidstore/crdts/default_hook.h
    include/fluidstore/crdts/delta_hook.h
    include/fluidstore/crdts/delta_replica.h  
    include/fluidstore/crdts/dot.h
    include/fluidstore/crdts/dot_context.h
    include/fluidstore/crdts/dot_kernel.h
    include/fluidstore/crdts/id_sequence.h
    include/fluidstore/crdts/set.h
    include/fluidstore/crdts/map.h
    include/fluidstore/crdts/value.h
    include/fluidstore/crdts/value_mv.h
    include/fluidstore/crdts/noncopyable.h
    include/fluidstore/crdts/registry.h
    include/fluidstore/crdts/replica.h
    include/fluidstore/crdts/tagged_allocator.h
    include/fluidstore/crdts/tagged_collection.h
    include/fluidstore/allocators/arena_allocator.h
)

find_package(Boost)
if(Boost_FOUND)
    add_executable(fluidstoretest 
        src/tests/dot.cpp
        src/tests/map.cpp
        src/tests/performance.cpp
        #src/tests/replica.cpp
        src/tests/set.cpp
        src/tests/testmodule.cpp
        src/tests/value_mv.cpp
        src/tests/pch.h
    )
    add_test(fluidstoretest COMMAND fluidstoretest)
    target_link_libraries(fluidstoretest fluidstore Boost::boost)
    target_precompile_headers(fluidstoretest PRIVATE src/tests/pch.h)
endif()