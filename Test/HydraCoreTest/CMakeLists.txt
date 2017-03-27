include_directories(${CMAKE_SOURCE_DIR}/HydraCore)

add_executable( GCIntergretedTest
    GCIntergretedTest.cpp
    TestHeapObject.h
)
target_link_libraries( GCIntergretedTest HydraCore )
add_test(GCIntergretedTest GCIntergretedTest)

add_executable( GCTest
    GCTest.cpp
    TestHeapObject.h
)
target_link_libraries( GCTest HydraCore )
add_test(GCTest GCTest)

add_executable( GCStopTheWorldTest
    GCStopTheWorldTest
    TestHeapObject.h
)
target_link_libraries( GCStopTheWorldTest HydraCore)
add_test(GCStopTheWorldTest GCStopTheWorldTest)
