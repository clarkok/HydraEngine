#include "JSArray.h"

namespace hydra
{
namespace runtime
{

bool JSArray::Get(size_t key, JSValue &value, JSObjectPropertyAttribute &attribute)
{
    if (key < SplitPoint)
    {
        attribute = TablePart->at(key << 1).SmallInt();
        if (!attribute.HasValue())
        {
            return false;
        }

        value = TablePart->at((key << 1) + 1);
        return true;
    }

    return GetSlowInternal(key, value, attribute);
}

void JSArray::Set(
    gc::ThreadAllocator &allocator,
    size_t key,
    JSValue value,
    JSObjectPropertyAttribute attribute)
{
    auto heap = gc::Heap::GetInstance();

    if (key < SplitPoint)
    {
        TablePart->at(key << 1) = attribute;
        TablePart->at((key << 1) + 1) = value;

        if (value.IsReference())
        {
            heap->WriteBarrier(this, value.ToReference());
        }
    }
    else
    {
        SetSlowInternal(allocator, key, value, attribute);
    }
}

void JSArray::Delete(size_t key)
{
    if (key < SplitPoint)
    {
        TablePart->at(key << 1) = JSObjectPropertyAttribute();
        TablePart->at((key << 1) + 1) = JSValue();
    }
    else
    {
        DeleteSlowInternal(key);
    }
}

bool JSArray::GetSlowInternal(size_t key, JSValue &value, JSObjectPropertyAttribute &attribute)
{
    return GetInHash(HashPart, key, value, attribute);
}

void JSArray::SetSlowInternal(
    gc::ThreadAllocator &allocator,
    size_t key,
    JSValue value,
    JSObjectPropertyAttribute attribute)
{
    auto heap = gc::Heap::GetInstance();

    if (key < SplitPoint + 2)
    {
        auto newSplitPoint = key;
        if (TablePart->Capacity() <= newSplitPoint * 2)
        {
            auto newTablePart = Array::NewOfLevel(allocator, TablePart->GetLevel() + 1);
            std::copy(TablePart->begin(), TablePart->end(), newTablePart->begin());

            TablePart->Scan([&](gc::HeapObject *obj)
            {
                heap->WriteBarrier(newTablePart, obj);
            });
            TablePart = newTablePart;
            heap->WriteBarrier(this, TablePart);
        }

        if (SplitPoint + 1 != key)
        {
            JSValue value;
            JSObjectPropertyAttribute attribute;
            if (GetSlowInternal(SplitPoint + 1, value, attribute))
            {
                TablePart->at(((SplitPoint + 1) << 1)) = attribute;
                TablePart->at(((SplitPoint + 1) << 1) + 1) = value;
                DeleteSlowInternal(SplitPoint + 1);

                if (value.IsReference())
                {
                    heap->WriteBarrier(TablePart, value.ToReference());
                }
            }
        }

        SplitPoint = newSplitPoint;
        TablePart->at(key << 1) = attribute;
        TablePart->at((key << 1) + 1) = value;

        if (value.IsReference())
        {
            heap->WriteBarrier(this, value.ToReference());
        }
    }
    else
    {
        if (!SetInHash(HashPart, key, value, attribute))
        {
            auto newHashPart = Array::NewOfLevel(allocator, HashPart->GetLevel() + 1);
            Rehash(newHashPart, HashPart);

            HashPart = newHashPart;
            heap->WriteBarrier(this, HashPart);

            auto result = SetInHash(HashPart, key, value, attribute);
            hydra_assert(result, "must succeed");
        }
    }
}

void JSArray::DeleteSlowInternal(size_t key)
{
    JSValue value;
    JSObjectPropertyAttribute attribute;
    if (GetInHash(HashPart, key, value, attribute))
    {
        SetInHash(HashPart, key, JSValue(), attribute);
    }
}

bool JSArray::GetInHash(Array *dstHash, size_t key, JSValue &value, JSObjectPropertyAttribute &attribute)
{
    auto hashTableCapacity = dstHash->Capacity() / 2;
    auto index = key % hashTableCapacity;

    do
    {
        JSValue attr = dstHash->at((index << 1));
        JSObjectPropertyAttribute currentAttribute;
        size_t currentKey;

        SplitAttrSlot(attr, currentAttribute, currentKey);

        if (!currentAttribute.HasValue())
        {
            return false;
        }
        else if (currentKey == key)
        {
            attribute = currentAttribute;
            value = dstHash->at((index << 1) + 1);
            return true;
        }
    } while ((index = (index + 1) % hashTableCapacity) != key % hashTableCapacity);
    return false;
}

bool JSArray::SetInHash(Array *dstHash, size_t key, JSValue value, JSObjectPropertyAttribute attribute)
{
    auto heap = gc::Heap::GetInstance();
    auto hashTableCapacity = dstHash->Capacity() / 2;
    auto index = key % hashTableCapacity;

    do
    {
        JSValue attr = dstHash->at((index << 1));
        JSObjectPropertyAttribute currentAttribute;
        size_t currentKey;

        SplitAttrSlot(attr, currentAttribute, currentKey);

        if (!currentAttribute.HasValue() || currentKey == key)
        {
            dstHash->at((index << 1)) = MergeAttrSlot(attribute, key);
            dstHash->at((index << 1) + 1) = value;
            if (value.IsReference())
            {
                heap->WriteBarrier(dstHash, value.ToReference());
            }
            return true;
        }
    } while ((index = (index + 1) % hashTableCapacity) != key % hashTableCapacity);

    return false;
}

void JSArray::Rehash(Array *dstHash, Array *srcHash)
{
    auto heap = gc::Heap::GetInstance();
    auto srcHashTableCapacity = srcHash->Capacity() / 2;

    for (size_t index = 0; index < srcHashTableCapacity; ++index)
    {
        JSValue attr = srcHash->at((index << 1));
        JSObjectPropertyAttribute currentAttribute;
        size_t currentKey;
        JSValue value = srcHash->at((index << 1) + 1);

        SplitAttrSlot(attr, currentAttribute, currentKey);

        if (currentAttribute.HasValue())
        {
            auto result = SetInHash(dstHash, currentKey, value, currentAttribute);
            hydra_assert(result, "must succeeded");
        }
    }
}

} // namespace runtime
} // namespace hydra