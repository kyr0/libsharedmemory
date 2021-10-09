cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(LSM)

set(header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/libsharedmemory/libsharedmemory.hpp)

add_library(lsm INTERFACE)
target_sources(lsm INTERFACE "$<BUILD_INTERFACE:${header_files}>")

target_include_directories(lsm INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)
target_include_directories(lsm SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)

if (${CMAKE_GENERATOR} MATCHES "Visual")
    target_compile_options(lsm INTERFACE -W3 -EHsc)
else()
    target_compile_options(lsm INTERFACE -Wall -Wno-missing-braces -std=c++11 -fPIC)
endif()

option(LSM_BUILD_TEST "build test" ON)
if(${LSM_BUILD_TEST} AND (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
    enable_testing()
    add_subdirectory(test/)
endif()