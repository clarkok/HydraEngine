#ifndef _TEST_HEAP_OBJECT_H_
#define _TEST_HEAP_OBJECT_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/Heap.h"
#include "GarbageCollection/ThreadAllocator.h"

struct TestHeapObject : public hydra::gc::HeapObject
{
    TestHeapObject(
        hydra::u8 property,
        TestHeapObject *ref1 = nullptr,
        TestHeapObject *ref2 = nullptr,
        TestHeapObject *ref3 = nullptr)
        : HeapObject(property),
        Ref1(ref1),
        Ref2(ref2),
        Ref3(ref3),
        Id(Count())
    {
        if (ref1) hydra::gc::Heap::GetInstance()->WriteBarrier(this, ref1);
        if (ref2) hydra::gc::Heap::GetInstance()->WriteBarrier(this, ref2);
        if (ref3) hydra::gc::Heap::GetInstance()->WriteBarrier(this, ref3);
    }

    ~TestHeapObject()
    {
        Id = 0;
        Ref1 = Ref2 = Ref3 = nullptr;
    }

    TestHeapObject *Ref1;
    TestHeapObject *Ref2;
    TestHeapObject *Ref3;
    size_t Id;

    virtual void Scan(std::function<void(hydra::gc::HeapObject *)> scan) override
    {
        if (Ref1) scan(Ref1);
        if (Ref2) scan(Ref2);
        if (Ref3) scan(Ref3);
    }

    static inline size_t Count()
    {
        static size_t counter = 0;
        return counter++;
    }
};

#endif // _TEST_HEAP_OBJECT_H_