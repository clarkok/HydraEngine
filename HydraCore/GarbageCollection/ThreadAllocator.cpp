#include "ThreadAllocator.h"

#include "Common/Platform.h"

namespace hydra
{
namespace gc
{

void ThreadAllocator::ThreadScan()
{
    auto perfSession = Logger::GetInstance()->Perf("ThreadScan");

    platform::ForeachWordOnStack([](void **stackPtr)
    {
        void *ptr = *stackPtr;
        Cell *cell = nullptr;
        if (Region::IsInRegion(ptr, cell))
        {
            if (cell && cell->IsInUse())
            {
                Heap::GetInstance()->Remember(dynamic_cast<HeapObject*>(cell));
            }
        }
    });
}

} // namespace gc
} // namespace hydra