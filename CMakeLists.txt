cmake_minimum_required(VERSION 3.5)

project(fixparser)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_TESTS "Build test suit" ON)
option(WITH_CONAN "Resolving the dependencies with Conan" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message("Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif()

set(FIXP_SOURCES src/fixparser.hpp)

include("cmake/cmakeconan.cmake")

add_library(fixparser ${FIXP_SOURCES})
set_target_properties(fixparser PROPERTIES LINKER_LANGUAGE CXX)

# Building and resolving dependencies with CONAN?
if(WITH_CONAN)
    target_link_libraries(fixparser ${CONAN_LIBS} stdc++fs) 
else()
    
    message("Not building with CONAN")
    
    find_package(pugixml REQUIRED)
    target_link_libraries(fixparser pugixml stdc++fs)

endif()

target_include_directories(fixparser PRIVATE src)

# Installing targets 

export(TARGETS ${CMAKE_PROJECT_NAME}
       NAMESPACE ${CMAKE_PROJECT_NAME}::
       FILE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}Config.cmake")

install(TARGETS ${CMAKE_PROJECT_NAME}
        EXPORT ${CMAKE_PROJECT_NAME}Config
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(FILES ${FIXP_SOURCES} DESTINATION include)
install (FILES "spec/FIX44.xml"
        DESTINATION etc/fixparser)

install(EXPORT ${CMAKE_PROJECT_NAME}Config
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/${CMAKE_PROJECT_NAME}/cmake"
        NAMESPACE ${CMAKE_PROJECT_NAME}::)


if(BUILD_EXAMPLES)
    add_subdirectory(example)   
endif()
                       
