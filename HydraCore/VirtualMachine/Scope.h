#ifndef _SCOPE_H_
#define _SCOPE_H_

#include "Runtime/Type.h"
#include "Runtime/ManagedArray.h"

#include "GarbageCollection/HeapObject.h"

#include <vector>

namespace hydra
{
namespace vm
{

class Scope : public gc::HeapObject
{
protected:
    using Array = runtime::Array;
    using RangeArray = runtime::RangeArray;
    using JSValue = runtime::JSValue;

public:
    Scope(u8 property,
        Scope *upper,
        Array *regs,
        Array *table,
        RangeArray *captured,
        JSValue thisArg,
        RangeArray *arguments
    ) : gc::HeapObject(property),
        Upper(upper),
        Regs(regs),
        Table(table),
        Captured(captured),
        ThisArg(thisArg),
        Arguments(arguments),
        Allocated(0)
    {
        gc::Heap::GetInstance()->WriteBarrier(this, upper);
        gc::Heap::GetInstance()->WriteBarrier(this, regs);
        gc::Heap::GetInstance()->WriteBarrier(this, table);
        gc::Heap::GetInstance()->WriteBarrier(this, captured);
        if (ThisArg.IsReference() && ThisArg.ToReference())
        {
            gc::Heap::GetInstance()->WriteBarrier(this, ThisArg.ToReference());
        }
        gc::Heap::GetInstance()->WriteBarrier(this, arguments);
    }

    Scope(const Scope &) = delete;
    Scope(Scope &&) = delete;
    Scope &operator = (const Scope &) = delete;
    Scope &operator = (Scope &&) = delete;

    virtual ~Scope() = default;

    virtual void Scan(std::function<void(gc::HeapObject *)> scan);

    inline Scope *GetUpper() const
    {
        return Upper;
    }

    inline Array *GetRegs() const
    {
        return Regs;
    }

    inline Array *GetTable() const
    {
        return Table;
    }

    inline RangeArray *GetCaptured() const
    {
        return Captured;
    }

    inline JSValue GetThisArg() const
    {
        return ThisArg;
    }

    inline RangeArray *GetArguments() const
    {
        return Arguments;
    }

    inline JSValue *Allocate()
    {
        hydra_assert(Allocated < Table->Capacity(),
            "Too many allocations");
        return &Table->at(Allocated++);
    }

    static JSValue *AllocateStatic(Scope *scope);

    inline static size_t OffsetUpper()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->Upper);
    }

    inline static size_t OffsetRegs()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->Regs);
    }

    inline static size_t OffsetTable()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->Table);
    }

    inline static size_t OffsetCaptured()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->Captured);
    }

    inline static size_t OffsetThisArg()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->ThisArg);
    }

    inline static size_t OffsetArguments()
    {
        return reinterpret_cast<uintptr_t>(
            &reinterpret_cast<Scope*>(0)->Arguments);
    }

    static void Safepoint(Scope *scope);

    static thread_local Scope *ThreadTop;

protected:
    Scope *Upper;
    Array *Regs;
    Array *Table;
    RangeArray *Captured;
    JSValue ThisArg;
    RangeArray *Arguments;
    size_t Allocated;
};

struct AutoThreadTop
{
    AutoThreadTop(Scope *scope)
    {
        OldThreadTop = Scope::ThreadTop;
        Scope::ThreadTop = scope;
    }

    ~AutoThreadTop()
    {
        Scope::ThreadTop = OldThreadTop;
    }

    Scope *OldThreadTop;
};

} // namespace vm
} // namespace hydra

#endif // _SCOPE_H_