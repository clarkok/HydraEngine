#include "ThreadAllocator.h"

#include "Common/Platform.h"
#include "Runtime/Type.h"

namespace hydra
{
namespace gc
{

using runtime::JSValue;

void ThreadAllocator::ThreadScan()
{
    auto perfSession = Logger::GetInstance()->Perf("ThreadScan");

    platform::ForeachWordOnStack([](void **stackPtr)
    {
        void *ptr = nullptr;
        const JSValue &value = JSValue::AtAddress(stackPtr);

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
                Heap::GetInstance()->Remember(dynamic_cast<HeapObject*>(cell));
            }
        }
        else if (Heap::GetInstance()->IsLargeObject(ptr))
        {
            Heap::GetInstance()->Remember(reinterpret_cast<HeapObject*>(ptr));
        }
    });
}

} // namespace gc
} // namespace hydra