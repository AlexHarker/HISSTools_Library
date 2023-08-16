cmake_minimum_required(VERSION 3.13)

# library
project(
    hisstools 
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

#tests
option(BUILD_TESTS OFF)

if(BUILD_TESTS)
    enable_testing()

    if(BUILD_TEST)
        set(TESTS ${BUILD_TEST})
    else()
        file(GLOB TESTS test/*.cpp)
    endif()

    foreach(TEST ${TESTS})
        get_filename_component(TEST_NAME ${TEST} NAME_WE)

        add_executable(
            ${TEST_NAME}
            ${TEST}
        )

        if (WIN32)
            target_compile_definitions(
                ${TEST_NAME} 
                PRIVATE 
                    NOMINMAX 
                    _USE_MATH_DEFINES
            )
        endif()

        target_link_libraries(${TEST_NAME} ${PROJECT_NAME})

        add_test(
            NAME ${TEST_NAME} 
            COMMAND $<TARGET_FILE:${TEST_NAME}>
        )
    endforeach()
endif(BUILD_TESTS)

#install
include(GNUInstallDirs)

if(DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "${CMAKE_SOURCE_DIR}/install"
        CACHE PATH "Where the library will be installed to" FORCE
    )
endif()

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}>
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}_Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
    ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION
    ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(
    FILES 
        ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake 
    DESTINATION 
        ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(
    EXPORT ${PROJECT_NAME}_Targets
    FILE ${PROJECT_NAME}Targets.cmake
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/ 
    DESTINATION include/${PROJECT_NAME}
)
