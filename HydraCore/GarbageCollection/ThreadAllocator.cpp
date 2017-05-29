#include "ThreadAllocator.h"

#include "Common/Platform.h"
#include "Runtime/Type.h"
#include "VirtualMachine/Scope.h"

namespace hydra
{
namespace gc
{

using runtime::JSValue;

void ThreadAllocator::ThreadScan()
{
    auto perfSession = Logger::GetInstance()->Perf("ThreadScan");
    auto heap = Heap::GetInstance();

    platform::ForeachWordOnStack([heap](void **stackPtr)
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
                heap->Remember(dynamic_cast<HeapObject*>(cell));
            }
        }
        else if (heap->IsLargeObject(ptr))
        {
            heap->Remember(reinterpret_cast<HeapObject*>(ptr));
        }
    });

    /*
    auto scope = vm::Scope::ThreadTop;
    std::set<HeapObject *> scanned;

    while (scope)
    {
#define REMEMBER(_memb)                                                             \
        if (scope->Get ## _memb() &&                                                \
            (scanned.find(scope->Get ## _memb()) != scanned.end()))                 \
        {                                                                           \
            for (auto &value : *(scope->Get ## _memb()))                            \
            {                                                                       \
                if (value.IsReference() && value.ToReference())                     \
                {                                                                   \
                    heap->Remember(value.ToReference());                            \
                }                                                                   \
            }                                                                       \
        }

        REMEMBER(Regs);
        REMEMBER(Table);
        REMEMBER(Captured);
        REMEMBER(Arguments);

        if (scope->GetThisArg().IsReference() && scope->GetThisArg().ToReference())
        {
            heap->Remember(scope->GetThisArg().ToReference());
        }

        scope = scope->GetUpper();
    }
    */
}

} // namespace gc
} // namespace hydra