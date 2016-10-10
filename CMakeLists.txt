cmake_minimum_required(VERSION 3.3)
project(HydraEngine)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
