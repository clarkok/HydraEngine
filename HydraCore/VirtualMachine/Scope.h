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
    using JSValue = runtime::JSValue;

public:
    Scope(u8 property,
        Scope *upper,
        Array *regs,
        Array *table,
        Array *captured,
        JSValue thisArg,
        Array *arguments
    ) : gc::HeapObject(property),
        Upper(upper),
        Regs(regs),
        Table(table),
        Captured(std::move(captured)),
        ThisArg(thisArg),
        Arguments(arguments),
        Allocated(0)
    {
        gc::Heap::GetInstance()->WriteBarrier(this, upper);
        gc::Heap::GetInstance()->WriteBarrier(this, regs);
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

    inline Array *GetCaptured() const
    {
        return Captured;
    }

    inline JSValue GetThisArg() const
    {
        return ThisArg;
    }

    inline Array *GetArguments() const
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

protected:
    Scope *Upper;
    Array *Regs;
    Array *Table;
    Array *Captured;
    JSValue ThisArg;
    Array *Arguments;
    size_t Allocated;
};

} // namespace vm
} // namespace hydra

#endif // _SCOPE_H_