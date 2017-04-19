#ifndef _KLASS_H_
#define _KLASS_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/GC.h"

#include "RuntimeDefs.h"
#include "Type.h"
#include "String.h"
#include "JSObject.h"   // we need sizeof(JSObject)
#include "ManagedHashMap.h"

namespace hydra
{
namespace runtime
{

class Klass : public gc::HeapObject
{
public:
    static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    static inline size_t TableSizeFromLevel(size_t level)
    {
        return (gc::Region::CellSizeFromLevel(level) - sizeof(Klass)) / sizeof(String *);
    }

    Klass *AddTransaction(gc::ThreadAllocator &allocator, String *key)
    {
        auto currentTransaction = Transaction.load();

        if (!currentTransaction)
        {
            auto newTransaction = KlassTransaction::New(
                allocator, DEFAULT_TRANSACTION_SIZE_IN_KLASS);

            Transaction.compare_exchange_strong(currentTransaction, newTransaction);
            currentTransaction = Transaction.load();
        }

        auto newLevel = Level;
        if (KeyCount == TableSize)
        {
            newLevel = Level + 1;
        }

        auto newKlass = allocator.AllocateWithSizeAuto<Klass>(
            gc::Region::CellSizeFromLevel(newLevel),
            newLevel,
            this,
            key,
            std::ref(allocator)
        );

        bool found = false;
        bool set = false;
        
        while (!currentTransaction->FindOrSet(key, newKlass, found, set))
        {
            currentTransaction->Rehash(allocator);
        }

        return newKlass;
    }

    bool Find(String *key, size_t &index)
    {
        if (KeyCount <= MINIMAL_KEY_COUNT_TO_ENABLE_HASH_IN_KLASS)
        {
            for (size_t i = 0; i < KeyCount; ++i)
            {
                if (Table()[i]->EqualsTo(key))
                {
                    index = i;
                    return true;
                }
            }
        }
        else
        {
            hydra_assert(IndexMap, "IndexMap must not null");

            if (IndexMap->Find(key, index))
            {
                return true;
            }
        }

        return false;
    }

    static Klass *EmptyKlass(gc::ThreadAllocator &allocator);

    inline String **begin()
    {
        return Table();
    }

    inline String **end()
    {
        return Table() + KeyCount;
    }

    inline size_t size() const
    {
        return KeyCount;
    }

    template <typename T, typename ...T_Args>
    T *NewObject(gc::ThreadAllocator &allocator, T_Args ...args)
    {
        static_assert(std::is_base_of<JSObject, T>::value,
            "T must be inheritted from JSObject");
        auto table = Array::New(allocator, KeyCount * 2);

        return allocator.AllocateAuto<T>(this, table, args...);
    }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final
    {
        if (IndexMap)
        {
            scan(IndexMap);
        }

        if (Transaction)
        {
            scan(Transaction);
        }

        for (auto key : *this)
        {
            if (key)
            {
                scan(key);
            }
        }
    }

private:
    using KlassTransaction = HashMap<Klass *>;

    Klass(u8 property, size_t level)
        : HeapObject(property),
        Level(level),
        TableSize(TableSizeFromLevel(level)),
        KeyCount(0),
        IndexMap(nullptr),
        Transaction(nullptr)
    {
        std::memset(Table(), 0, TableSize * sizeof(String*));
    }

    Klass(u8 property, size_t level, Klass *other, String *key, gc::ThreadAllocator &allocator)
        : Klass(property, level)
    {
        auto heap = gc::Heap::GetInstance();

        for (auto otherKey : *other)
        {
            Table()[KeyCount++] = otherKey;
            heap->WriteBarrier(this, otherKey);
        }

        Table()[KeyCount++] = key;
        heap->WriteBarrier(this, key);

        if (KeyCount > MINIMAL_KEY_COUNT_TO_ENABLE_HASH_IN_KLASS)
        {
            IndexMap = HashMap<size_t>::New(allocator, KeyCount);
            heap->WriteBarrier(this, IndexMap);

            for (size_t i = 0; i < KeyCount; ++i)
            {
                auto result = IndexMap->TrySet(Table()[i], i);
                hydra_assert(result, "Must succeed");
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
    HashMap<size_t> *IndexMap;
    std::atomic<KlassTransaction *> Transaction;

    friend class gc::Region;
};

} // namespace runtime
} // namespace hydra

#endif // _KLASS_H_