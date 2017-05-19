#ifndef _VM_DEFS_H_
#define _VM_DEFS_H_

#include "Common/HydraCore.h"

namespace hydra
{

namespace gc
{
class ThreadAllocator;
}

namespace runtime
{
struct JSValue;
}

namespace vm
{

class Scope;

using GeneratedCode = bool(*)(gc::ThreadAllocator &allocator,
    Scope *scope,
    runtime::JSValue &retVal,
    runtime::JSValue &error);

} // namespace vm
} // namespace hydra

#endif // _VM_DEFS_H_