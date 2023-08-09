cmake_minimum_required(VERSION 3.13)

project(
    HISSTools_Library 
    VERSION 1.0 
    LANGUAGES CXX
    DESCRIPTION "A C++ Library for DSP"
)

add_library(${PROJECT_NAME} INTERFACE)

set_target_properties(
    ${PROJECT_NAME} 
    PROPERTIES
    VERSION ${PROJECT_VERSION}
)

target_compile_features(
    ${PROJECT_NAME} 
    INTERFACE 
        cxx_std_11
)

include(GNUInstallDirs)

set(CMAKE_INSTALL_PREFIX
    "${CMAKE_SOURCE_DIR}/install"
    CACHE PATH "Where the library will be installed to" FORCE
)

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION install
)
