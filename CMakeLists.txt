project(fluidstore)

cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
enable_testing()

add_library(fluidstore INTERFACE)
target_include_directories(fluidstore INTERFACE include)

target_sources(fluidstore INTERFACE
    include/fluidstore/crdts/dot.h
    include/fluidstore/allocators/arena_allocator.h
)

find_package(Boost)
if(Boost_FOUND)
    add_executable(fluidstoretest 
        src/tests/dot.cpp
    )
    add_test(fluidstoretest COMMAND fluidstoretest)
    target_link_libraries(fluidstoretest fluidstore Boost::boost)
endif()