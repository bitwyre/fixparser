cmake_minimum_required(VERSION 3.0)

project(fixparser VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)

aux_source_directory(src FIXP_SOURCES)
 
include("cmake/cmakeconan.cmake")
set(BUILD_EXE "Build the executable" ON)

if(BUILD_EXE)

    add_executable(fixparser ${FIXP_SOURCES})
    target_link_libraries(fixparser ${CONAN_LIBS} stdc++fs) 

else()

    add_library(fixparserl ${FIXP_SOURCES})
    target_link_libraries(fixparserl ${CONAN_LIBS} stdc++fs)

endif()

add_custom_command(TARGET fixparser POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/spec/ $<TARGET_FILE_DIR:fixparser>/spec)
