cmake_minimum_required(VERSION 3.15)

# The version lives in one place only: include/heatshrink_common.h
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/heatshrink_common.h"
    _hs_version_lines
    REGEX "^#define HEATSHRINK_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
string(REGEX REPLACE ".*HEATSHRINK_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1"
    _hs_version_major "${_hs_version_lines}")
string(REGEX REPLACE ".*HEATSHRINK_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
    _hs_version_minor "${_hs_version_lines}")
string(REGEX REPLACE ".*HEATSHRINK_VERSION_PATCH[ \t]+([0-9]+).*" "\\1"
    _hs_version_patch "${_hs_version_lines}")

project(heatshrink
    VERSION ${_hs_version_major}.${_hs_version_minor}.${_hs_version_patch}
    DESCRIPTION "LZSS compression library for embedded and real-time systems"
    HOMEPAGE_URL "https://github.com/atomicobject/heatshrink"
    LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(HEATSHRINK_BUILD_CLI "Build the heatshrink command line tool" ON)
option(HEATSHRINK_BUILD_TESTS "Build the heatshrink test suite" ON)
option(HEATSHRINK_USE_INDEX "Use the search index (faster, more RAM)" ON)
option(HEATSHRINK_WERROR "Treat compiler warnings as errors" OFF)

set(HEATSHRINK_SOURCES
    src/heatshrink_encoder.c
    src/heatshrink_decoder.c)

set(HEATSHRINK_PUBLIC_HEADERS
    include/heatshrink_common.h
    include/heatshrink_config.h
    include/heatshrink_encoder.h
    include/heatshrink_decoder.h)

if(NOT MSVC)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(MSVC)
    # Shared libraries built with MSVC need explicit exports; in C we do not
    # have an export header, so let CMake generate one for us.
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Warnings common to all targets. These are applied directly (instead of
# linking an INTERFACE target) so that the installed export set does not need
# to reference a build-only target.
if(MSVC)
    set(HS_WARNING_FLAGS /W4)
    if(HEATSHRINK_WERROR)
        list(APPEND HS_WARNING_FLAGS /WX)
    endif()
else()
    set(HS_WARNING_FLAGS -Wall -Wextra -pedantic
        -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations)
    if(HEATSHRINK_WERROR)
        list(APPEND HS_WARNING_FLAGS -Werror)
    endif()
endif()

function(hs_set_warnings target)
    target_compile_options(${target} PRIVATE ${HS_WARNING_FLAGS})
endfunction()

# Two flavours of the library: with and without dynamic allocation.
# BUILD_SHARED_LIBS selects static (.a/.lib) or shared (.so/.dll/.dylib).
foreach(_hs_alloc IN ITEMS static dynamic)
    if(_hs_alloc STREQUAL "static")
        set(_hs_alloc_value 0)
    else()
        set(_hs_alloc_value 1)
    endif()

    add_library(heatshrink_${_hs_alloc} ${HEATSHRINK_SOURCES})
    add_library(heatshrink::${_hs_alloc} ALIAS heatshrink_${_hs_alloc})

    target_include_directories(heatshrink_${_hs_alloc} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_definitions(heatshrink_${_hs_alloc} PUBLIC
        HEATSHRINK_DYNAMIC_ALLOC=${_hs_alloc_value}
        HEATSHRINK_USE_INDEX=$<BOOL:${HEATSHRINK_USE_INDEX}>)
    hs_set_warnings(heatshrink_${_hs_alloc})

    set_target_properties(heatshrink_${_hs_alloc} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})
endforeach()

# A convenience target that always refers to the dynamically allocating
# library, matching what the command line tool uses.
add_library(heatshrink INTERFACE)
add_library(heatshrink::heatshrink ALIAS heatshrink)
target_link_libraries(heatshrink INTERFACE heatshrink_dynamic)

if(HEATSHRINK_BUILD_CLI)
    add_executable(heatshrink_cmd src/heatshrink.c)
    set_target_properties(heatshrink_cmd PROPERTIES OUTPUT_NAME heatshrink)
    target_link_libraries(heatshrink_cmd PRIVATE heatshrink_dynamic)
    hs_set_warnings(heatshrink_cmd)
    if(MSVC)
        target_compile_definitions(heatshrink_cmd PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

if(HEATSHRINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation
set(_hs_install_targets heatshrink_static heatshrink_dynamic)
if(HEATSHRINK_BUILD_CLI)
    list(APPEND _hs_install_targets heatshrink_cmd)
endif()

install(TARGETS ${_hs_install_targets}
    EXPORT heatshrinkTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${HEATSHRINK_PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(_hs_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/heatshrink)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/heatshrinkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/heatshrinkConfig.cmake
    INSTALL_DESTINATION ${_hs_cmake_dir})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/heatshrinkConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT heatshrinkTargets
    FILE heatshrinkTargets.cmake
    NAMESPACE heatshrink::
    DESTINATION ${_hs_cmake_dir})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/heatshrinkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/heatshrinkConfigVersion.cmake
    DESTINATION ${_hs_cmake_dir})
