add_library(HydraCore.GarbageCollection OBJECT
    GCDefs.h
    GC.h
    Heap.cpp
    Heap.h
    HeapObject.h
    Region.cpp
    Region.h
    ThreadAllocator.cpp
    ThreadAllocator.h
    GCScheduler.cpp
    GCScheduler.h
)
