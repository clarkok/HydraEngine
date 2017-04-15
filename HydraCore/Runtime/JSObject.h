#ifndef _JSOBJECT_H_
#define _JSOBJECT_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "ManagedHashMap.h"
#include "Type.h"
#include "String.h"

namespace hydra
{
namespace runtime
{

class Klass;

/* This class is not completely thread-safe */
class JSObject : public gc::HeapObject
{
public:
    JSObject(u8 property, Klass *klass);

    inline Klass *GetKlass() const
    {
        if (Replacement)
        {
            return Replacement->GetKlass();
        }

        return Klass;
    }

    static JSObject *Latest(JSObject *& obj)
    {
        while (obj->Replacement)
        {
            obj = obj->Replacement;
        }
        return obj;
    }

    JSValue Get(String *key);
    bool Set(String *key, JSValue value);
    void Add(gc::ThreadAllocator &allocator, String *key, JSValue value);
    void Delete(String *key);
    bool Index(String *key, size_t &index);

    inline bool Offset(String *key, size_t &offset)
    {
        size_t index;
        if (Index(key, index))
        {
            offset = Index2Offset(index);
            return true;
        }
        return false;
    }

    virtual void Scan(std::function<void(HeapObject*)> scan) override;

private:
    JSObject(u8 property, Klass *klass, JSObject *other);

    JSValue &Slot(size_t index);
    JSValue *Table();
    JSValue *TableLimit();

    static size_t Index2Offset(size_t index)
    {
        return sizeof(JSObject) + sizeof(JSValue) * index;
    }

    Klass *Klass;
    JSObject *Replacement;

    friend class gc::Region;
};

static_assert((sizeof(JSObject) + 3 * sizeof(JSValue)) <= gc::MINIMAL_ALLOCATE_SIZE,
    "MINIMAL_ALLOCATE_SIZE is too small");

} // namespace runtime
} // namespace hydra

#endif // _JSOBJECT_H_