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

add_executable(comet_multi_thread_bench
    benchmarks/allocator_multi_thread_bench.cpp
)

target_link_libraries(comet_multi_thread_bench
    PRIVATE comet
)


add_executable(comet_dashboard
    dashboard/backend/server.cpp
    dashboard/backend/workload.cpp
    dashboard/backend/telemetry.cpp
    dashboard/backend/event_logger.cpp
    dashboard/allocator/allocator.cpp
)

target_include_directories(comet_dashboard PRIVATE
    dashboard/backend
    dashboard/allocator
    include
)

target_compile_features(comet_dashboard PRIVATE cxx_std_17)

target_link_libraries(comet_dashboard PRIVATE Threads::Threads)



# Separate allocator build for dashboard diagnostics.
# Tests and benchmarks continue to use the normal `comet` target.
add_library(comet_diagnostic
    src/allocator.cpp
)

target_include_directories(comet_diagnostic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/dashboard/backend/diagnostics
)

target_compile_definitions(comet_diagnostic
    PRIVATE COMET_ENABLE_DIAGNOSTICS
)

target_link_libraries(comet_diagnostic
    PUBLIC Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(comet PRIVATE -Wall -Wextra -Wpedantic)
endif()

