#include "ThreadAllocator.h"

#include "Common/Platform.h"
#include "Runtime/Type.h"
#include "VirtualMachine/Scope.h"

namespace hydra
{
namespace gc
{

struct ThreadAllocatorInitializeHelper : public Singleton<ThreadAllocatorInitializeHelper>
{
    ThreadAllocatorInitializeHelper()
    {
        ThreadAllocator::Initialize();
    }
};

using runtime::JSValue;

std::set<ThreadAllocator *> ThreadAllocator::InactiveSets;
std::mutex ThreadAllocator::InactiveSetsMutex;

void ThreadAllocator::Initialize()
{
    gc::Heap::GetInstance()->RegisterRootScanFunc(ScanAllInactiveThreads);
}

void ThreadAllocator::SetInactive()
{
    ThreadAllocatorInitializeHelper::GetInstance();

    platform::StackState state = platform::GetCurrentStackState();
    SetInactive([=]()
    {
        platform::ForeachWordOnStackWithState(state, ScanWordOnStack);
    });
}

void ThreadAllocator::ThreadScan()
{
    auto perfSession = Logger::GetInstance()->Perf("ThreadScan");
    platform::ForeachWordOnStack(ScanWordOnStack);
}

void ThreadAllocator::ScanWordOnStack(void **stackPtr)
{
    auto heap = Heap::GetInstance();
    void *ptr = nullptr;
    JSValue value = JSValue::AtAddress(stackPtr);

    if (value.IsReference())
    {
        ptr = value.ToReference();
    }
    else
    {
        ptr = *stackPtr;
    }

    Cell *cell = nullptr;
    if (Region::IsInRegion(ptr, cell))
    {
        if (cell && cell->IsInUse())
        {
            hydra_assert(dynamic_cast<HeapObject*>(cell),
                "must be an HeapObject");
            heap->Remember(dynamic_cast<HeapObject*>(cell));
        }
    }
    else if (heap->IsLargeObject(ptr))
    {
        heap->Remember(reinterpret_cast<HeapObject*>(ptr));
    }
}

void ThreadAllocator::ScanAllInactiveThreads(std::function<void(gc::HeapObject *)> scan)
{
    std::unique_lock<std::mutex> lck(InactiveSetsMutex);

    for (auto &allocator : InactiveSets)
    {
        allocator->ReporterFunction();
    }
}

} // namespace gc
} // namespace hydra