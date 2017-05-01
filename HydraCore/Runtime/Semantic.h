#ifndef _SEMAMTIC_H_
#define _SEMANTIC_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "RuntimeDefs.h"
#include "JSObject.h"
#include "JSArray.h"

namespace hydra
{
namespace runtime
{
namespace semantic
{

void RootScan(std::function<void(gc::HeapObject*)> scan);

void Initialize(gc::ThreadAllocator &allocator);

// Object & Array
JSObject *NewEmptyObject(gc::ThreadAllocator &allocator);
JSObject *NewObject(gc::ThreadAllocator &allocator);
JSArray *NewArray(gc::ThreadAllocator &allocator, size_t capacity = DEFAULT_JSARRAY_SPLIT_POINT);

} // namespace semantic
} // namespace runtime
} // namespace hydra

#endif // _SEMANTIC_H_