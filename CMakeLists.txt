project(fluidstore)

cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
add_link_options("$<$<CXX_COMPILER_ID:MSVC>:LINKER:/DEBUG:FASTLINK>")
#add_link_options("$<$<CXX_COMPILER_ID:Clang>:-static-libstdc++>")
enable_testing()

add_library(fluidstore INTERFACE)
target_include_directories(fluidstore INTERFACE include)

target_sources(fluidstore INTERFACE
    include/fluidstore/btree/detail/container.h
    include/fluidstore/btree/detail/fixed_vector.h
    include/fluidstore/btree/map.h
    include/fluidstore/btree/set.h
    include/fluidstore/crdt/allocator.h
    include/fluidstore/crdt/detail/allocator_ptr.h
    include/fluidstore/crdt/detail/dot.h
    include/fluidstore/crdt/detail/dot_kernel.h
    include/fluidstore/crdt/detail/dot_kernel_allocator.h
    include/fluidstore/crdt/detail/dot_kernel_iterator.h
    include/fluidstore/crdt/detail/dot_kernel_metadata.h
    include/fluidstore/crdt/detail/dot_kernel_metadata_btree.h
    include/fluidstore/crdt/detail/dot_kernel_metadata_flat.h
    include/fluidstore/crdt/detail/dot_kernel_metadata_stl.h
    include/fluidstore/crdt/detail/dot_kernel_value.h
    include/fluidstore/crdt/detail/traits.h
    include/fluidstore/crdt/hooks/hook_default.h
    include/fluidstore/crdt/hooks/hook_extract.h
    include/fluidstore/crdt/set.h
    include/fluidstore/crdt/map.h
    include/fluidstore/crdt/replica.h    
    include/fluidstore/crdt/tags.h
    include/fluidstore/crdt/value_mv.h    
    include/fluidstore/flat/map.h
    include/fluidstore/flat/memory.h
    include/fluidstore/flat/set.h
    include/fluidstore/flat/vector.h
    include/fluidstore/memory/buffer_allocator.h
    readme.md
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
        src/tests/btree/btree.cpp
        src/tests/btree/fixed_vector.cpp
        src/tests/crdt/map.cpp
        src/tests/crdt/performance.cpp
        src/tests/crdt/set.cpp
        src/tests/crdt/value_mv.cpp
        src/tests/flat/flat.cpp
        src/tests/memory/buffer_allocator.cpp
        src/tests/testmodule.cpp
        src/tests/pch.h
        src/tests/bench.h
    )
    add_test(fluidstoretest COMMAND fluidstoretest)
    target_link_libraries(fluidstoretest fluidstore Boost::boost)
    target_include_directories(fluidstoretest PRIVATE src/tests)
    target_precompile_headers(fluidstoretest PRIVATE src/tests/pch.h)    

    if(TLX_INCLUDE)
        target_include_directories(fluidstoretest PRIVATE ${TLX_INCLUDE})
        target_compile_definitions(fluidstoretest PRIVATE TLX_ENABLED)
    endif()
        
    if(WIN32)
        group_sources(src)
        group_sources(include)
    endif()
endif()