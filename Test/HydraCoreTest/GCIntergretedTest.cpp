#include "Common/HydraCore.h"
#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/Heap.h"
#include "GarbageCollection/ThreadAllocator.h"

#include "TestHeapObject.h"

#include <iostream>

using namespace hydra;
using namespace std::chrono_literals;

int main()
{
    std::cout << "Hello World" << std::endl;

    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    auto started = std::chrono::system_clock::now();

    // while ((std::chrono::system_clock::now() - started) < 1min)
    while (true)
    {
        TestHeapObject *head = nullptr;
        size_t count = 1000;

        while (count--)
        {
            head = allocator.Allocate<TestHeapObject>(
                [&]()
                {
                    Logger::GetInstance()->Log() << "Report local references";
                    heap->Remember(head);
                },
                head);
        }

        TestHeapObject *ptr = head;
        size_t lastId = ptr->Id;
        count = 1;
        while (ptr->Ref1)
        {
            ptr = ptr->Ref1;
            hydra_assert(ptr->Id == lastId - 1,
                "Id should match");
            lastId = ptr->Id;
            count++;
        }

        hydra_assert(count == 1000, "Count should match");
    }

    allocator.SetInactive([]() {});

    heap->Shutdown();
    Logger::GetInstance()->Shutdown();

    return 0;
}