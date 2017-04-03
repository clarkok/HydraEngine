#ifndef _KLASS_H_
#define _KLASS_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "Type.h"
#include "String.h"
#include "JSObject.h"   // we need sizeof(JSObject)

namespace hydra
{
namespace runtime
{

class KlassTransaction;

class Klass : public gc::HeapObject
{
public:
    static inline size_t JSObjectSlotSizeOfLevel(size_t level)
    {
        hydra_assert(level < gc::LEVEL_NR, "level has a limit");
        return (gc::Region::CellSizeFromLevel(level) - sizeof(JSObject)) / sizeof(JSValue);
    }

    static inline size_t KlassTableSizeOfLevel(size_t level)
    {
        hydra_assert(level < gc::LEVEL_NR, "level has a limit");
        return JSObjectSlotSizeOfLevel(level) * sizeof(Slot);
    }

private:
    struct Slot
    {
    public:
        Slot(String *key, Type type)
            : Value(reinterpret_cast<u64>(key) | static_cast<u8>(type))
        {
            hydra_assert((reinterpret_cast<u64>(key) & cexpr::Mask(0, 3)) == 0,
                "Last 3 bits of key should be zero");
            hydra_assert(static_cast<u8>(type) < 8,
                "Invalid type");
        }

        Type GetType() const
        {
            return static_cast<Type>(Value & cexpr::Mask(0, 3));
        }

        String *GetString() const
        {
            return reinterpret_cast<String*>(Value & ~cexpr::Mask(0, 3));
        }

        bool Empty() const
        {
            return !!Value;
        }

    private:
        u64 Value;
    };

    Klass(u8 property, size_t level)
        : HeapObject(property), Level(level), TableSize(JSObjectSlotSizeOfLevel(level)), KeyCount(0)
    { }

    Klass(u8 property, size_t level, Klass *other, String *key, Type type)
        : Klass(property, level)
    {
        if (other->Level == level)
        {
            Slot *otherBegin = other->Table();
            Slot *otherEnd = other->Table() + other->TableSize;

            Slot *table = Table();
            std::copy(otherBegin, otherEnd, table);

            Slot newSlot(key, type);
            u64 hash = key->GetHash();
            u64 index = hash % TableSize;

            while (!table[index].Empty() && !table[index].GetString()->EqualsTo(key))
            {
                index = (index + 1) % TableSize;
                hydra_assert(index != hash % TableSize,
                    "table is not big enough");
            }

            if (table[index].Empty())
            {
                table[index] = newSlot;
                KeyCount = other->KeyCount + 1;
            }
            else
            {
                KeyCount = other->KeyCount;
            }
        }
        else
        {
            Slot *otherBegin = other->Table();
            Slot *otherEnd = other->Table() + other->TableSize;

            Slot *table = Table();
            std::memset(table, 0, sizeof(Slot) * TableSize);

            for (Slot *s = otherBegin; s != otherEnd; ++s)
            {
                if (s->Empty())
                {
                    continue;
                }

                u64 hash = s->GetString()->GetHash();
                u64 index = hash % TableSize;

                while (!table[index].Empty())
                {
                    index = (index + 1) % TableSize;
                    hydra_assert(index != hash % TableSize,
                        "table is not big enough");
                }

                table[index] = *s;
            }

            Slot newSlot(key, type);
            u64 hash = key->GetHash();
            u64 index = hash % TableSize;

            while (!table[index].Empty() && !table[index].GetString()->EqualsTo(key))
            {
                index = (index + 1) % TableSize;
                hydra_assert(index != hash % TableSize,
                    "table is not big enough");
            }

            if (table[index].Empty())
            {
                table[index] = newSlot;
                KeyCount = other->KeyCount + 1;
            }
            else
            {
                KeyCount = other->KeyCount;
            }
        }

        {
            // invoking WriteBarrier
            auto table = Table();
            for (Slot *s = table; s != table + TableSize; ++s)
            {
                if (s)
                {
                    gc::Heap::GetInstance()->WriteBarrier(this, s->GetString());
                }
            }
        }
    }

    Slot *Table()
    {
        return reinterpret_cast<Slot *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(Klass));
    }

    size_t Level;
    size_t TableSize;
    size_t KeyCount;
    std::atomic<KlassTransaction *> Transaction;
};

} // namespace runtime
} // namespace hydra

#endif // _KLASS_H_