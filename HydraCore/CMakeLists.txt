include_directories(${CMAKE_SOURCE_DIR}/HydraCore)

add_subdirectory(Common)
add_subdirectory(GarbageCollection)
add_subdirectory(Runtime)
add_subdirectory(VirtualMachine)

add_library( HydraCore STATIC
    HydraCore.cpp
    $<TARGET_OBJECTS:HydraCore.Common>
    $<TARGET_OBJECTS:HydraCore.GarbageCollection>
    $<TARGET_OBJECTS:HydraCore.Runtime>
    $<TARGET_OBJECTS:HydraCore.VirtualMachine>
)
