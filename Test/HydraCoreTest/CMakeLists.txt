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

add_executable( ConcurrentQueueTest
    ConcurrentQueueTest.cpp
)
target_link_libraries( ConcurrentQueueTest HydraCore )
add_test(ConcurrentQueueTest ConcurrentQueueTest)

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

add_executable( ThreadPoolTest
    ThreadPoolTest.cpp
)
target_link_libraries( ThreadPoolTest HydraCore )
add_test(ThreadPoolTest ThreadPoolTest)

add_executable( JSObjectTest
    JSObjectTest.cpp
)
target_link_libraries( JSObjectTest HydraCore )
add_test(JSObjectTest JSObjectTest)

add_executable( JSArrayTest
    JSArrayTest.cpp
)
target_link_libraries( JSArrayTest HydraCore )
add_test(JSArrayTest JSArrayTest)

add_executable( ManagedArrayTest
    ManagedArrayTest.cpp
)
target_link_libraries( ManagedArrayTest HydraCore )
add_test(ManagedArrayTest ManagedArrayTest)

add_executable( ByteCodeTest
    ByteCodeTest.cpp
)
target_link_libraries( ByteCodeTest HydraCore SHLWAPI)
add_test(ByteCodeTest ByteCodeTest)
