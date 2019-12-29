cmake_minimum_required(VERSION 3.5)
project(example)
 
set(CMAKE_CXX_STANDARD 17)
find_package(fixparser CONFIG REQUIRED)

add_executable(example main.cpp)

if(CONAN_LIBS)
    target_link_libraries(example ${CONAN_LIBS} stdc++fs)
else()
    target_link_libraries(example pugixml fixparser stdc++fs)
endif()