cmake_minimum_required(VERSION 2.8)
project(glextract)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "-std=c++14 -Wall")
add_executable(glextract glextract.cpp glextract_const.cpp)
target_link_libraries(glextract boost_regex)
