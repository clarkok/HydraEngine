include_directories(${CMAKE_SOURCE_DIR}/HydraCore)

add_subdirectory(Common)
add_subdirectory(Runtime)
add_subdirectory(GarbageCollection)

add_library( HydraCore STATIC
    HydraCore.cpp
    $<TARGET_OBJECTS:HydraCore.Common>
    $<TARGET_OBJECTS:HydraCore.GarbageCollection>
)
