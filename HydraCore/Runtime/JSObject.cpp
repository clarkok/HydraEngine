#include "JSObject.h"

#include "Klass.h"

namespace hydra
{
namespace runtime
{

bool JSObject::Get(String *key, JSValue &value, JSObjectPropertyAttribute &attribute)
{
    size_t index = 0;
    if (Index(key, index))
    {
        attribute = Table->at(index << 1).SmallInt();
        if (!attribute.HasValue())
        {
            return false;
        }

        value = Table->at((index << 1) + 1);
        return true;
    }
    return false;
}

void JSObject::Set(gc::ThreadAllocator &allocator,
    String *key,
    JSValue value,
    JSObjectPropertyAttribute attribute)
{
    auto heap = gc::Heap::GetInstance();

    size_t index = 0;
    if (!Index(key, index))
    {
        Klass = Klass->AddTransaction(allocator, key);
        heap->WriteBarrier(this, Klass);

        auto result = Index(key, index);
        hydra_assert(result, "Must have an index");

        if (Klass->size() * 2 > Table->Capacity())
        {
            auto newTable = Array::NewOfLevel(allocator, Table->GetLevel() + 1);
            std::copy(Table->begin(), Table->end(), newTable->begin());
            newTable->Scan([&](gc::HeapObject *ref)
            {
                heap->WriteBarrier(newTable, ref);
            });
            Table = newTable;
            heap->WriteBarrier(this, Table);
        }
    }

    Table->at(index << 1) = JSValue::FromSmallInt(attribute);
    Table->at((index << 1) + 1) = value;
    if (value.IsReference())
    {
        heap->WriteBarrier(Table, value.ToReference());
    }
}

void JSObject::Delete(String *key)
{
    size_t index = 0;
    if (Index(key, index))
    {
        Table->at(index << 1) = JSObjectPropertyAttribute();
        Table->at((index << 1) + 1) = JSValue();
    }
}

bool JSObject::Index(String *key, size_t &index)
{
    return Klass->Find(key, index);
}

void JSObject::Scan(std::function<void(gc::HeapObject*)> scan)
{
    scan(Klass);
    scan(Table);
}

}
}
