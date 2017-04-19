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

        if (value.Type() == runtime::Type::T_OBJECT)
        {
            ptr = value.Object();
        }
        else if (value.Type() == runtime::Type::T_STRING)
        {
            ptr = value.String();
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