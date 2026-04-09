cmake_minimum_required(VERSION 3.16)
project(benchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark Qt6::Core)
