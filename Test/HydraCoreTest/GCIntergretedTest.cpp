#include "Common/HydraCore.h"
#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/Heap.h"
#include "GarbageCollection/ThreadAllocator.h"

#include "TestHeapObject.h"

#include <iostream>

using namespace hydra;
using namespace std::chrono_literals;

namespace hydra
{
class ThreadAllocatorTester
{
public:
    static void ThreadScan()
    {
        gc::ThreadAllocator::ThreadScan();
    }
};
}

int main()
{
    std::cout << "Hello World" << std::endl;

    gc::Heap *heap = gc::Heap::GetInstance();
    gc::ThreadAllocator allocator(heap);

    auto started = std::chrono::system_clock::now();

    constexpr size_t ROUND = 10000000;

    size_t round = 0;

    std::array<gc::Cell *, 100> headers;
    std::fill(headers.begin(), headers.end(), nullptr);

    while (true)
    {
        if (round++ % 8192 == 0)
        {
            std::cout << std::chrono::high_resolution_clock::now().time_since_epoch().count() << std::endl;
        }

        TestHeapObject *head = nullptr;
        size_t count = 1000;

        while (count--)
        {
            head = allocator.AllocateAuto<TestHeapObject>(head);
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

        headers[round % headers.size()] = head;
        for (auto header : headers)
        {
            if (!header) { continue; }

            TestHeapObject *ptr = dynamic_cast<TestHeapObject*>(header);
            hydra_assert(ptr, "dynamic_cast failed");

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

        if (round % 100 == 0)
        {
            Logger::GetInstance()->Log() << "Round " << (ROUND - round);
        }
    }

    allocator.SetInactive([]() {});

    auto ended = std::chrono::system_clock::now();
    Logger::GetInstance()->Log() << "Test Finished: " << std::chrono::duration_cast<std::chrono::milliseconds>(ended - started).count() << "ms";

    heap->Shutdown();
    Logger::GetInstance()->Shutdown();

    return 0;
}