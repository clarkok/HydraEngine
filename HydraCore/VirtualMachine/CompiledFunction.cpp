#include "CompiledFunction.h"

namespace hydra
{
namespace vm
{

bool CompiledFunction::Call(
    gc::ThreadAllocator &allocator,
    Scope *scope,
    runtime::JSValue &retVal,
    runtime::JSValue &error)
{
    return true;
}

bool UncompiledFunction::Call(
    gc::ThreadAllocator &allocator,
    Scope *scope,
    runtime::JSValue &retVal,
    runtime::JSValue &error)
{
    return true;
}

}
}