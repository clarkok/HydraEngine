#ifndef _JSOBJECT_H_
#define _JSOBJECT_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "ManagedHashMap.h"
#include "ManagedArray.h"
#include "Type.h"
#include "String.h"
#include "Klass.h"

namespace hydra
{
namespace runtime
{

enum class ObjectType
{
    OT_OBJECT,
    OT_ARRAY,
    OT_FUNCTION
};

struct JSObjectPropertyAttribute
{
    constexpr static u64 HAS_VALUE = 1;
    constexpr static u64 IS_DATA_MASK = 1u << 1;
    constexpr static u64 IS_CONFIGURABLE_MASK = 1u << 2;
    constexpr static u64 IS_ENUMERABLE_MASK = 1u << 3;
    constexpr static u64 IS_WRITABLE_MASK = 1u << 4;

    constexpr static u64 DEFAULT_DATA_ATTRIBUTE =
        HAS_VALUE | IS_DATA_MASK | IS_CONFIGURABLE_MASK | IS_ENUMERABLE_MASK | IS_WRITABLE_MASK;

    constexpr static size_t USED_BITS = 5;

    JSObjectPropertyAttribute(u64 payload = 0)
        : Payload(payload)
    { }

    operator u64 () const
    {
        return Payload & cexpr::Mask(0, USED_BITS);
    }

    operator JSValue () const
    {
        return JSValue::FromSmallInt(Payload);
    }

    inline bool HasValue() const
    {
        return (Payload & HAS_VALUE) != 0;
    }

    inline bool IsData() const
    {
        return (Payload & IS_DATA_MASK) != 0;
    }

    inline bool IsConfigurable() const
    {
        return (Payload & IS_CONFIGURABLE_MASK) != 0;
    }

    inline bool IsEnumerable() const
    {
        return (Payload & IS_ENUMERABLE_MASK) != 0;
    }

    inline bool IsWritable() const
    {
        return (Payload & IS_WRITABLE_MASK) != 0;
    }

    u64 Payload;
};

/* This class is not completely thread-safe */
class JSObject : public gc::HeapObject
{
public:
    JSObject(u8 property, Klass *klass, Array *table)
        : HeapObject(property), Klass(klass), Table(table)
    { }

    inline Klass *GetKlass() const
    {
        return Klass;
    }

    virtual ObjectType GetType() const
    {
        return ObjectType::OT_OBJECT;
    }

    bool Get(String *key, JSValue &value, JSObjectPropertyAttribute &attribute);
    void Set(gc::ThreadAllocator &allocator,
        String *key,
        JSValue value,
        JSObjectPropertyAttribute attribute = JSObjectPropertyAttribute::DEFAULT_DATA_ATTRIBUTE);
    void Delete(String *key);

    bool Index(String *key, size_t &index);

    inline static auto GetTable()
    {
        return &JSObject::Table;
    }

    virtual void Scan(std::function<void(HeapObject*)> scan) override;

private:
    Klass *Klass;
    Array *Table;

    friend class Klass;
    friend class gc::Region;
};

} // namespace runtime
} // namespace hydra

#endif // _JSOBJECT_H_
