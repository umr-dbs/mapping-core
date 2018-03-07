# Project
project(mapping_core
        LANGUAGES CXX
)

# CMake Settings
cmake_minimum_required(VERSION 3.7)
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

## Debug
set(CMAKE_VERBOSE_MAKEFILE OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/target/lib)

# User Options
set(MAPPING_MODULE_PATH .. CACHE PATH "A list of mapping modules to include in the build process")
set(MAPPING_MODULES "" CACHE STRING "A list of mapping modules to include in the build process")

# Copy default settings file to binary directory
configure_file(${CMAKE_CURRENT_LIST_DIR}/settings-default.toml ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ COPYONLY)

# External Dependencies
include(DownloadProject)

## Core
download_project(PROJ       jsoncpp
        GIT_REPOSITORY      https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG             1.7.7
        UPDATE_DISCONNECTED 1
        PREFIX CMakeFiles/Download
        )
add_subdirectory(${jsoncpp_SOURCE_DIR} ${jsoncpp_BINARY_DIR} EXCLUDE_FROM_ALL)

download_project(PROJ       cpptoml
        GIT_REPOSITORY      https://github.com/skystrife/cpptoml.git
        GIT_TAG             master
        UPDATE_DISCONNECTED 1
        PREFIX CMakeFiles/Download
        )
add_subdirectory(${cpptoml_SOURCE_DIR} ${cpptoml_BINARY_DIR} EXCLUDE_FROM_ALL)

add_subdirectory(src)

include(CTest)
enable_testing()
add_subdirectory(test)