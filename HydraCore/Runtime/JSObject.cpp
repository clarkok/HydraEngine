#include "JSObject.h"

#include "Klass.h"

namespace hydra
{
namespace runtime
{

JSObject::JSObject(u8 property, runtime::Klass *klass)
    : HeapObject(property), Klass(klass), Replacement(nullptr)
{
    gc::Heap::GetInstance()->WriteBarrier(this, klass);
}

void JSObject::Scan(std::function<void(HeapObject*)> scan)
{
    scan(Klass);
    if (Replacement) { scan(Replacement); }

    // TODO scan properties
}

JSValue &JSObject::Slot(size_t index)
{
    return reinterpret_cast<JSValue*>(
        reinterpret_cast<uintptr_t>(this) + sizeof(JSObject))[index];
}

JSValue JSObject::Get(String *key)
{
    if (Replacement)
    {
        return Replacement->Get(key);
    }

    size_t index;
    if (Index(key, index))
    {
        return Slot(index);
    }

    return JSValue();
}

bool JSObject::Has(String *key)
{
    if (Replacement)
    {
        return Replacement->Has(key);
    }

    size_t index;
    return Index(key, index) && Slot(index) != JSValue();
}

bool JSObject::Set(String *key, JSValue value)
{
    if (Replacement)
    {
        return Replacement->Set(key, value);
    }

    size_t index;
    if (Index(key, index))
    {
        Slot(index) = value;
        return true;
    }

    return false;
}

void JSObject::Add(gc::ThreadAllocator &allocator, String *key, JSValue value)
{
    if (Replacement)
    {
        Replacement->Add(allocator, key, value);
        return;
    }

    if (Set(key, value))
    {
        return;
    }

    auto newKlass = Klass->AddTransaction(allocator, key);
    if (newKlass->GetLevel() == Klass->GetLevel())
    {
        Klass = newKlass;
        gc::Heap::GetInstance()->WriteBarrier(this, Klass);
    }

    auto result = Set(key, value);
    hydra_assert(result, "must succeeded");
}

bool JSObject::Index(String *key, size_t &index)
{
    index = Klass->Find(key);
    return index != runtime::Klass::INVALID_OFFSET;
}

}
}