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
    INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/hisstools>
)

install(
    TARGETS ${PROJECT_NAME} 
    EXPORT HISSTools_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}
    DESTINATION include
)

# include(CMakePackageConfigHelpers)

# write_basic_package_version_file(
#     "HISSToolsConfigVersion.cmake"
#     VERSION ${PROJECT_VERSION}
#     COMPATIBILITY AnyNewerVersion
# )

# configure_package_config_file(
#     "${PROJECT_SOURCE_DIR}/cmake/HISSToolsConfig.cmake.in"
#     "${PROJECT_BINARY_DIR}/HISSToolsConfig.cmake"
#     INSTALL_DESTINATION
#         ${CMAKE_INSTALL_LIBDIR}/cmake/hisstools
# )

# install(
#     FILES 
#         ${PROJECT_BINARY_DIR}/HISSToolsConfig.cmake
#         ${CMAKE_CURRENT_BINARY_DIR}/HISSToolsConfigVersion.cmake 
#     DESTINATION 
#         ${CMAKE_INSTALL_LIBDIR}/cmake/hisstools
# )
