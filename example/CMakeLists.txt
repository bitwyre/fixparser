cmake_minimum_required(VERSION 3.5)
project(example)

find_library(fixparser REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE fixparser)