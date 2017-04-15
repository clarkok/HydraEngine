#ifndef _KLASS_H_
#define _KLASS_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "Type.h"
#include "String.h"
#include "JSObject.h"   // we need sizeof(JSObject)
#include "ManagedHashMap.h"

namespace hydra
{
namespace runtime
{

class KlassTransaction;

class Klass : public gc::HeapObject
{
public:
    static constexpr size_t INVALID_OFFSET = static_cast<size_t>(-1);

    static inline size_t JSObjectSlotSizeOfLevel(size_t level)
    {
        return (gc::Region::CellSizeFromLevel(level) - sizeof(JSObject)) / sizeof(JSValue);
    }

    static inline size_t KlassTableSizeOfLevel(size_t level)
    {
        return JSObjectSlotSizeOfLevel(level) * sizeof(String *);
    }

    static inline size_t KlassObjectSizeOfLevel(size_t level)
    {
        return KlassTableSizeOfLevel(level) * sizeof(String*) + sizeof(Klass);
    }

    size_t Find(String *key)
    {
        u64 hash = key->GetHash();
        u64 index = hash % TableSize;

        do
        {
            if (!Table()[index])
            {
                return INVALID_OFFSET;
            }
            else if (Table()[index]->EqualsTo(key))
            {
                return index;
            }
        } while ((index = (index + 1) % TableSize) != hash % TableSize);

        hydra_trap("Key not found in Klass!");
    }

    Klass *AddTransaction(gc::ThreadAllocator &allocator, String *key)
    {
        hydra_assert(Find(key) == INVALID_OFFSET,
            "Should not have key");

        KlassTransaction *currentTransaction = KlassTransaction::Latest(Transaction);
        if (!currentTransaction)
        {
            KlassTransaction *newTransaction = KlassTransaction::NewOfLevel(allocator, 1);

            // not care about result
            Transaction.compare_exchange_strong(currentTransaction, newTransaction);
        }

        Klass *newKlass = nullptr;

        if (currentTransaction->Find(key, newKlass))
        {
            return newKlass;
        }

        size_t newLevel = (KeyCount == TableSize) ? Level + 1 : Level;
        size_t newSize = KlassObjectSizeOfLevel(newLevel);
        newKlass = allocator.AllocateWithSizeAuto<Klass>(newSize, newLevel, this, key);

        // now care about result
        currentTransaction->FindOrSet(key, newKlass);
        return newKlass;
    }

    inline size_t GetLevel() const
    {
        return Level;
    }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final
    {
        KlassTransaction *currentTransaction = KlassTransaction::Latest(Transaction);
        if (currentTransaction)
        {
            scan(currentTransaction);
        }

        for (String **pKey = Table(); pKey != TableLimit(); ++pKey)
        {
            if (*pKey)
            {
                scan(*pKey);
            }
        }
    }

private:
    using KlassTransaction = HashMap<Klass *>;

    Klass(u8 property, size_t level)
        : HeapObject(property),
        Level(level),
        TableSize(JSObjectSlotSizeOfLevel(level)),
        KeyCount(0),
        Transaction(nullptr)
    { }

    Klass(u8 property, size_t level, Klass *other, String *key)
        : Klass(property, level)
    {
        auto Add = [this] (String *key)
        {
            u64 hash = key->GetHash();
            u64 index = hash % TableSize;

            do
            {
                if (!Table()[index])
                {
                    Table()[index] = key;
                    break;
                }
            } while ((index = (index + 1) % TableSize) != hash % TableSize);
        };

        if (other->Level == level)
        {
            std::copy(other->Table(), other->TableLimit(), Table());
            Add(key);
        }
        else
        {
            std::fill(Table(), TableLimit(), nullptr);

            for (String **pKey = other->Table(); pKey != other->TableLimit(); ++pKey)
            {
                Add(*pKey);
            }
            Add(key);
        }

        for (String **pKey = Table(); pKey != TableLimit(); ++pKey)
        {
            if (*pKey)
            {
                gc::Heap::GetInstance()->WriteBarrier(this, *pKey);
            }
        }
    }

    inline String **Table()
    {
        return reinterpret_cast<String **>(
            reinterpret_cast<uintptr_t>(this) + sizeof(Klass));
    }

    inline String **TableLimit()
    {
        return Table() + TableSize;
    }

    size_t Level;
    size_t TableSize;
    size_t KeyCount;
    std::atomic<KlassTransaction *> Transaction;

    friend class gc::Region;
};

} // namespace runtime
} // namespace hydra

#endif // _KLASS_H_