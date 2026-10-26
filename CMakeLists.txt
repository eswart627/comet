cmake_minimum_required(VERSION 3.16)

project(Comet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(comet 
    src/allocator.cpp
)

target_include_directories(comet 
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)

target_link_libraries(comet
    PUBLIC Threads::Threads
)

add_executable(comet_tests
    tests/allocator_tests.cpp
)

target_link_libraries(comet_tests
    PRIVATE comet
)

add_executable(comet_edge_tests
    tests/allocator_edge_cases.cpp
)

target_link_libraries(comet_edge_tests
    PRIVATE comet
)

add_executable(comet_free_tests
    tests/allocator_free_tests.cpp
)

target_link_libraries(comet_free_tests
    PRIVATE comet
)

add_executable(comet_concurrency_tests
    tests/allocator_concurrency_tests.cpp
)

target_link_libraries(comet_concurrency_tests
    PRIVATE comet Threads::Threads
)

add_executable(comet_benchmark
    benchmarks/allocator_benchmark.cpp
)

target_link_libraries(comet_benchmark
    PRIVATE comet
)

add_executable(comet_free_bytes_count
    tests/allocator_free_bytes_count.cpp
)

target_link_libraries(comet_free_bytes_count
    PRIVATE comet
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(comet PRIVATE -Wall -Wextra -Wpedantic)
endif()

