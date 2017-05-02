#ifndef _SEMAMTIC_H_
#define _SEMANTIC_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "RuntimeDefs.h"
#include "JSObject.h"
#include "JSArray.h"
#include "JSFunction.h"

namespace hydra
{
namespace runtime
{
namespace semantic
{

void RootScan(std::function<void(gc::HeapObject*)> scan);

void Initialize(gc::ThreadAllocator &allocator);

/********************************** Object & Array ******************************/
// {}
JSObject *NewEmptyObjectSafe(gc::ThreadAllocator &allocator);
bool NewEmptyObject(gc::ThreadAllocator &allocator, JSValue &retVal, JSValue &error);

// new Ctor(...)
bool NewObjectSafe(gc::ThreadAllocator &allocator, JSFunction *constructor, JSArray *arguments, JSValue &retVal, JSValue &error);
bool NewObject(gc::ThreadAllocator &allocator, JSValue constructor, JSArray *arguments, JSValue &retVal, JSValue &error);

JSArray *NewArrayInternal(gc::ThreadAllocator &allocator, size_t capacity = DEFAULT_JSARRAY_SPLIT_POINT);
inline bool NewArray(gc::ThreadAllocator &allocator, JSValue &retVal, JSValue &error)
{
    js_return(JSValue::FromObject(NewArrayInternal(allocator)));
}

template<typename T_iterator>
bool NewArray(gc::ThreadAllocator &allocator, T_iterator begin, T_iterator end, JSValue &retVal, JSValue &error)
{
    size_t capacity = std::distance(begin, end);
    auto ret = NewArrayInternal(allocator, capacity);

    size_t index = 0;
    for (auto iter = begin; iter != end; ++iter)
    {
        ret->Set(allocator, index++, *iter);
    }

    js_return(ret);
}

bool ObjectGet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &retVal, JSValue &error);
bool ObjectGetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue &retVal, JSValue &error);
bool ObjectGetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue &retVal, JSValue &error);


/********************************** String ******************************/
bool IsStringIntegral(String *str, i64 &value);
bool IsStringNumeric(String *str, double &value, bool integral);
bool ToString(gc::ThreadAllocator &allocator, JSValue value, JSValue &retVal, JSValue &error);

} // namespace semantic
} // namespace runtime
} // namespace hydra

#endif // _SEMANTIC_H_