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

    std::array<TestHeapObject *, 10> headers;
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
            /*
            hydra::ThreadAllocatorTester::ThreadScan();
            hydra_assert(head->GetGCState() != gc::GCState::GC_WHITE,
                "must be scanned");
            /*
            head = allocator.Allocate<TestHeapObject>([&]()
            {
                auto heap = gc::Heap::GetInstance();

                for (auto &header : headers)
                {
                    if (header)
                    {
                        heap->Remember(header);
                    }
                }
                if (head)
                {
                    heap->Remember(head);
                }
            }, head);
            */
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

        headers[round % 10] = head;
        // headers[0] = head;

        hydra_assert(count == 1000, "Count should match");

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