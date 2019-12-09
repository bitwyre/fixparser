cmake_minimum_required(VERSION 3.0)

project(fixparser VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

aux_source_directory(src FIXP_SOURCES)
 
include("cmake/cmakeconan.cmake")
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/spec)

add_executable(fixparser ${FIXP_SOURCES})

target_link_libraries(fixparser ${CONAN_LIBS}) 
 
add_library(fixparserl ${FIXP_SOURCES})

target_link_libraries(fixparserl ${CONAN_LIBS})

