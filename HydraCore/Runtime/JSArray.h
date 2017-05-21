#ifndef _JSARRAY_H_
#define _JSARRAY_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "JSObject.h"
#include "Klass.h"

namespace hydra
{
namespace runtime
{

class JSArray : public JSObject
{
public:
    JSArray(u8 property, runtime::Klass *klass, Array *table,
        Array *tablePart, Array *hashPart, size_t splitPoint = DEFAULT_JSARRAY_SPLIT_POINT)
        : JSObject(property, klass, table),
        TablePart(tablePart),
        HashPart(hashPart),
        SplitPoint(splitPoint),
        Length(0)
    {
        hydra_assert(TablePart->Capacity() >= SplitPoint * 2,
            "TablePart should be large enough");

        auto heap = gc::Heap::GetInstance();

        heap->WriteBarrier(this, TablePart);
        heap->WriteBarrier(this, HashPart);
    }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override
    {
        JSObject::Scan(scan);
        scan(TablePart);
        scan(HashPart);
    }

    bool Get(size_t key, JSValue &value, JSObjectPropertyAttribute &attribute);
    void Set(gc::ThreadAllocator &allocator,
        size_t key,
        JSValue value,
        JSObjectPropertyAttribute attribute = JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
    void Delete(size_t key);

    inline size_t GetSplitPoint() const
    {
        return SplitPoint;
    }

    static JSArray *New(gc::ThreadAllocator &allocator, size_t splitPoint = DEFAULT_JSARRAY_SPLIT_POINT);

    inline size_t GetLength() const
    {
        return Length;
    }

    inline void SetLength(size_t newLength)
    {
        Length = newLength;
    }

private:
    bool GetSlowInternal(size_t key, JSValue &value, JSObjectPropertyAttribute &attribute);
    void SetSlowInternal(gc::ThreadAllocator &allocator,
        size_t key,
        JSValue value,
        JSObjectPropertyAttribute attribute = JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
    void DeleteSlowInternal(size_t key);

    static inline void SplitAttrSlot(JSValue slot, JSObjectPropertyAttribute &attribute, size_t &key)
    {
        auto smallInt = slot.SmallInt();
        attribute = smallInt & cexpr::Mask(0, JSObjectPropertyAttribute::USED_BITS);
        key = cexpr::SubBits(smallInt,
            JSObjectPropertyAttribute::USED_BITS,
            cexpr::TypeBitCount<u64>::value - JSObjectPropertyAttribute::USED_BITS);
    }

    static inline JSValue MergeAttrSlot(JSObjectPropertyAttribute attribute, size_t key)
    {
        return JSValue::FromSmallInt(attribute.operator size_t() |
            (key << JSObjectPropertyAttribute::USED_BITS));
    }

    static bool GetInHash(Array *dstHash, size_t key, JSValue &value, JSObjectPropertyAttribute &attribute);
    static bool SetInHash(Array *dstHash, size_t key, JSValue value, JSObjectPropertyAttribute attribute);
    static void Rehash(Array *dstHash, Array *srcHash);

    Array *TablePart;
    Array *HashPart;
    size_t SplitPoint;
    size_t Length;
};

} // namespace runtime
} // namespace hydra

#endif // _JSARRAY_H_