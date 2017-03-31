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

    constexpr size_t ROUND = 100000;

    size_t round = ROUND;

    // std::vector<TestHeapObject *> headers;
    std::array<TestHeapObject *, 10> headers;
    std::fill(headers.begin(), headers.end(), nullptr);

    while (round--)
    // while (true)
    {
        TestHeapObject *head = nullptr;
        size_t count = 1000;

        while (count--)
        {
            if (count & 1)
            {
                head = allocator.Allocate<TestHeapObject>(
                    [&]()
                    {
                        auto perfSession = Logger::GetInstance()->Perf("LambdaReport");
                        heap->Remember(head);
                        for (auto &header : headers)
                        {
                            heap->Remember(header);
                        }
                    },
                    head);
            }
            else
            {
                head = allocator.AllocateAuto<TestHeapObject>(head);
            }
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

        // headers.push_back(head);
        headers[round % 10] = head;

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