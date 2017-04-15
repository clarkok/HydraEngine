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

    struct iterator
    {
        iterator(Klass *owner = nullptr, size_t index = 0)
            : Owner(owner),
            Table(Owner ? Owner->Table() : nullptr),
            Limit(Owner ? Owner->TableLimit() - Owner->Table() : 0),
            Index(index)
        {
            if (Owner)
            {
                FindFirst();
            }
        }
        iterator(const iterator &) = default;
        iterator(iterator &&) = default;
        iterator & operator = (const iterator &) = default;
        iterator & operator = (iterator &&) = default;

        String * operator * () const
        {
            return Table[Index];
        }

        bool operator == (iterator other) const
        {
            return Owner == other.Owner && Index == other.Index;
        }

        bool operator != (iterator other) const
        {
            return !this->operator==(other);
        }

        iterator & operator ++()
        {
            FindNext();
            return *this;
        }

        iterator & operator --()
        {
            FindLast();
            return *this;
        }

        iterator operator ++(int)
        {
            auto ret = *this;
            this->operator++();
            return ret;
        }

        iterator operator --(int)
        {
            auto ret = *this;
            this->operator--();
            return ret;
        }

        bool Valid() const
        {
            return Owner && Index <= Limit;
        }

    private:
        inline void FindFirst()
        {
            hydra_assert(Owner, "Must have a valid Owner");

            for (; Index < Limit; ++Index)
            {
                if (Table[Index])
                {
                    break;
                }
            }
        }

        inline void FindNext()
        {
            if (!Valid())
            {
                return;
            }

            for (++Index; Index < Limit; ++Index)
            {
                if (Table[Index])
                {
                    return;
                }
            }
        }

        inline void FindLast()
        {
            if (!Valid())
            {
                return;
            }

            while (Index--)
            {
                if (Table[Index])
                {
                    return;
                }
            }

            Index = 0;
        }

        Klass *Owner;
        String **Table;
        size_t Limit;
        size_t Index;
    };

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

        return INVALID_OFFSET;
    }

    Klass *AddTransaction(gc::ThreadAllocator &allocator, String *key)
    {
        hydra_assert(Find(key) == INVALID_OFFSET,
            "Should not have key");

        KlassTransaction *currentTransaction = KlassTransaction::Latest(Transaction);
        if (!currentTransaction)
        {
            KlassTransaction *newTransaction = KlassTransaction::NewOfLevel(allocator, 1);

            if (Transaction.compare_exchange_strong(currentTransaction, newTransaction))
            {
                currentTransaction = newTransaction;
            }
        }

        Klass *newKlass = nullptr;

        if (currentTransaction->Find(key, newKlass))
        {
            return newKlass;
        }

        size_t newLevel = (KeyCount + 1 == TableSize) ? Level + 1 : Level;
        size_t newSize = KlassObjectSizeOfLevel(newLevel);
        newKlass = allocator.AllocateWithSizeAuto<Klass>(newSize, newLevel, this, key);

        bool found = false;
        bool set = false;
        while (!currentTransaction->FindOrSet(key, newKlass, found, set))
        {
            currentTransaction = currentTransaction->Rehash(allocator);
        }

        return newKlass;
    }

    inline size_t GetLevel() const
    {
        return Level;
    }

    static Klass *EmptyKlass(gc::ThreadAllocator &allocator);

    template <typename T, typename ...T_Args>
    T *NewObject(gc::ThreadAllocator &allocator, T_Args ...args)
    {
        static_assert(std::is_base_of<JSObject, T>::value,
            "T must be devired from JSObject");

        return allocator.AllocateWithSizeAuto<T>(
            gc::Region::CellSizeFromLevel(Level), this, args...);
    }

    inline iterator begin()
    {
        return iterator(this);
    }

    inline iterator end()
    {
        return iterator(this, TableSize);
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
    {
        std::memset(Table(), 0, TableSize * sizeof(String*));
    }

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

            gc::Heap::GetInstance()->WriteBarrier(this, key);
            KeyCount++;
        };

        for (String **pKey = other->Table(); pKey != other->TableLimit(); ++pKey)
        {
            if (*pKey)
            {
                Add(*pKey);
            }
        }
        Add(key);
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