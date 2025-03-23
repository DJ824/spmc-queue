cmake_minimum_required(VERSION 3.0)
project(SPMCQueue)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O3")

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(benchmark benchmark.cpp)

target_link_libraries(benchmark ${CMAKE_THREAD_LIBS_INIT})

set_target_properties(benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run_benchmark
    COMMAND ${CMAKE_BINARY_DIR}/bin/benchmark
    DEPENDS benchmark
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}")
