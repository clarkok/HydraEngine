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

add_executable( ManagedHashMapTest
    ManagedHashMapTest.cpp
)
target_link_libraries( ManagedHashMapTest HydraCore )
add_test(ManagedHashMapTest ManagedHashMapTest)

add_executable( KlassTest
    KlassTest.cpp
)
target_link_libraries( KlassTest HydraCore )
add_test(KlassTest KlassTest)
