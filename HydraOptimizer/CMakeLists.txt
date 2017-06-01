include_directories(${CMAKE_SOURCE_DIR}/HydraCore)

add_executable(HydraOptimizer HydraOptimizer.cpp)

target_link_libraries(HydraOptimizer HydraCore SHLWAPI)
