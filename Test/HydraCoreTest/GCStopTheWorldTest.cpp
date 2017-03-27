#include "Common/HydraCore.h"
#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/Heap.h"
#include "GarbageCollection/ThreadAllocator.h"

#include "TestHeapObject.h"

using namespace hydra;
using namespace std::chrono_literals;

std::atomic<bool> ShouldExit = { false };

void Thread()
{
    gc::ThreadCollector allocator(gc::Heap::GetInstance());

    while (!ShouldExit.load())
    {
        allocator.CheckIfNeedStop();
    }
}

int main()
{
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(Thread);
    }

    std::this_thread::sleep_for(3s);

    gc::Heap::GetInstance()->StopTheWorld();

    std::this_thread::sleep_for(5s);

    gc::Heap::GetInstance()->ResumeTheWorld();

    std::this_thread::sleep_for(2s);

    ShouldExit.store(true);

    for (auto &thread : threads)
    {
        thread.join();
    }

    return 0;
}
