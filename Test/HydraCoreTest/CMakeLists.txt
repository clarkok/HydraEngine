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
    GCStopTheWorldTest.cpp
    TestHeapObject.h
)
target_link_libraries( GCStopTheWorldTest HydraCore)
add_test(GCStopTheWorldTest GCStopTheWorldTest)

add_executable( StringTest
    StringTest.cpp
)
target_link_libraries( StringTest HydraCore )
add_test(StringTest StringTest)

add_executable( ConcurrentLevelHashSetTest
    ConcurrentLevelHashSetTest.cpp
)
target_link_libraries( ConcurrentLevelHashSetTest HydraCore )
add_test(ConcurrentLevelHashSetTest ConcurrentLevelHashSetTest)
