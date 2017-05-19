cmake_minimum_required(VERSION 3.4)
project(HydraEngine)

set(CMAKE_EXPORT_COMPILE_COMMANDS true)
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall --std=c++1z")
endif()

include_directories(${PROJECT_SOURCE_DIR})
include_directories(ThirdParty)

add_subdirectory(HydraCore)
add_subdirectory(HydraEngine)
add_subdirectory(Test)
