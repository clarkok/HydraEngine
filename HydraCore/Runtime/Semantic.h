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
namespace vm
{
struct IRInst;
class Scope;
}

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
bool NewObjectWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error);

// new Ctor(...)
bool NewObjectSafe(gc::ThreadAllocator &allocator, JSFunction *constructor, JSArray *arguments, JSValue &retVal, JSValue &error);
bool NewObject(gc::ThreadAllocator &allocator, JSValue constructor, JSArray *arguments, JSValue &retVal, JSValue &error);

bool Call(gc::ThreadAllocator &allocator, JSValue callee, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error);
bool GetGlobal(gc::ThreadAllocator &allocator, String *name, JSValue &retVal, JSValue &error);

JSArray *NewArrayInternal(gc::ThreadAllocator &allocator, size_t capacity = DEFAULT_JSARRAY_SPLIT_POINT);
inline bool NewArray(gc::ThreadAllocator &allocator, JSValue &retVal, JSValue &error)
{
    js_return(JSValue::FromObject(NewArrayInternal(allocator)));
}
bool NewArrayWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error);

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

bool NewFuncWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error);
bool NewArrowWithInst(gc::ThreadAllocator &allocator, vm::Scope *scope, vm::IRInst *inst, JSValue &retVal, JSValue &error);

bool ObjectGet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &retVal, JSValue &error);
bool ObjectGetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue &retVal, JSValue &error);
bool ObjectGetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue &retVal, JSValue &error);

bool ObjectSet(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue value, JSValue &error);
bool ObjectSetSafeObject(gc::ThreadAllocator &allocator, JSObject *object, String *key, JSValue value, JSValue &error);
bool ObjectSetSafeArray(gc::ThreadAllocator &allocator, JSArray *array, size_t key, JSValue value, JSValue &error);

bool ObjectDelete(gc::ThreadAllocator &allocator, JSValue object, JSValue key, JSValue &error);

/********************************** String ******************************/
bool IsStringIntegral(String *str, i64 &value);
bool ToString(gc::ThreadAllocator &allocator, JSValue value, JSValue &retVal, JSValue &error);

/******************************** Operation *****************************/
bool OpAdd(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpSub(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpMul(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpDiv(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpMod(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpBand(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpBor(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpBnot(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error);
bool OpLnot(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error);
bool OpSll(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpSrl(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpSrr(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpEq(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpEqq(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpNe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpNee(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpLt(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpLe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpGt(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpGe(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpIn(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpInstanceOf(gc::ThreadAllocator &allocator, JSValue a, JSValue b, JSValue &retVal, JSValue &error);
bool OpTypeOf(gc::ThreadAllocator &allocator, JSValue a, JSValue &retVal, JSValue &error);

vm::Scope *NewScope(gc::ThreadAllocator &allocator, vm::Scope *upper, vm::IRInst *inst);

bool ToBoolean(JSValue value);

} // namespace semantic
} // namespace runtime
} // namespace hydra

#endif // _SEMANTIC_H_