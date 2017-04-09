#ifndef _MANAGED_HASH_MAP_H_
#define _MANAGED_HASH_MAP_H_

#include "Common/HydraCore.h"
#include "Runtime/String.h"
#include "GarbageCollection/GC.h"

namespace hydra
{
namespace runtime
{

template <typename T>
class HashMap : public gc::HeapObject
{
public:
    HashMap(u8 property, size_t level, size_t tableSize)
        : HeapObject(property), Replacement(nullptr), KeyCount(0), Level(level), TableSize(tableSize)
    {
        auto table = Table();
        auto limit = table + TableSize;

        for (auto s = table; s != limit; ++s)
        {
            s->store(Slot());
        }
    }

    inline static HashMap *Latest(HashMap *&ptr)
    {
        if (!ptr)
        {
            return nullptr;
        }

        HashMap *next = nullptr;
        while ((next = ptr->Replacement.load()) != nullptr)
        {
            ptr = next;
        }
        return ptr;
    }

    inline static HashMap *Latest(std::atomic<HashMap *> &ptr)
    {
        HashMap *original = ptr.load();
        HashMap *current = original;
        HashMap *next = nullptr;

        if (!original)
        {
            return nullptr;
        }

        do
        {
            current = original;
            while ((next = current->Replacement.load()) != nullptr)
            {
                current = next;
            }
        } while (!ptr.compare_exchange_strong(original, current));
    }

    inline static HashMap *New(gc::ThreadAllocator &allocator, size_t capacity)
    {
        size_t sizeRequest = sizeof(HashMap) + sizeof(std::atomic<Slot>) * capacity;
        size_t levelRequest = gc::ThreadAllocator::GetLevelFromSize(sizeRequest);

        return NewOfLevel(allocator, levelRequest);
    }

    inline static HashMap *NewOfLevel(gc::ThreadAllocator &allocator, size_t level)
    {
        size_t capacity = (gc::Region::CellSizeFromLevel(level) - sizeof(HashMap)) / sizeof(std::atomic<Slot>);
        size_t allocateSize = sizeof(HashMap) + sizeof(std::atomic<Slot>) * capacity;

        return allocator.AllocateWithSizeAuto<HashMap>(allocateSize, level, capacity);
    }

    size_t Capacity() const
    {
        return TableSize;
    }

    virtual void Scan(std::function<void(gc::HeapObject*)> scan) override final
    {
        std::shared_lock<std::shared_mutex> lck(ReadWriteMutex);

        auto replacement = Replacement.load(std::memory_order_acquire);
        if (replacement)
        {
            scan(replacement);
        }

        auto table = Table();
        auto limit = table + TableSize;
        for (auto s = table; s != limit; ++s)
        {
            Slot slot = s->load(std::memory_order_acquire);
            if (slot.GetKey() == nullptr)
            {
                continue;
            }

            scan(slot.GetKey());
            ScanValue(slot.Value, scan);
        }
    }

    bool Find(String *key, T &value)
    {
        std::shared_lock<std::shared_mutex> lck(ReadWriteMutex);

        HashMap *replacement = Replacement.load(std::memory_order_acquire);
        if (replacement)
        {
            return replacement->Find(key, value);
        }

        auto hash = key->GetHash();
        size_t index = hash % TableSize;

        do
        {
            Slot slot = Table()[index].load(std::memory_order_acquire);
            if (slot.GetKey() == nullptr)
            {
                return false;
            }

            if (slot.GetKey()->EqualsTo(key))
            {
                if (slot.IsDeleted())
                {
                    return false;
                }

                value = slot.Value;
                return true;
            }
        } while ((index = (index + 1) % TableSize) != hash % TableSize);

        return false;
    }

    // return if set
    bool FindOrSet(String *key, T &value)
    {
        std::shared_lock<std::shared_mutex> lck(ReadWriteMutex);

        HashMap *replacement = Replacement.load(std::memory_order_acquire);
        if (replacement)
        {
            return replacement->FindOrSet(key, value);
        }

        auto hash = key->GetHash();
        size_t index = hash % TableSize;
        Slot slot(key, value);

        do
        {
            std::atomic<Slot> &pos = Table()[index];
            Slot current = pos.load(std::memory_order_acquire);

            while (current.GetKey() == nullptr)
            {
                if (pos.compare_exchange_strong(current, slot))
                {
                    gc::Heap::GetInstance()->WriteBarrier(this, key);
                    gc::Heap::GetInstance()->WriteBarrier(this, ValueToRef(value));

                    KeyCount.fetch_add(1, std::memory_order_relaxed);

                    return true;
                }
            }

            while (current.IsDeleted() && current.GetKey()->EqualsTo(key))
            {
                if (pos.compare_exchange_strong(current, slot))
                {
                    gc::Heap::GetInstance()->WriteBarrier(this, key);
                    gc::Heap::GetInstance()->WriteBarrier(this, ValueToRef(value));

                    KeyCount.fetch_add(1, std::memory_order_relaxed);

                    return true;
                }
            }

            if (current.GetKey()->EqualsTo(key))
            {
                value = current.Value;

                return false;
            }
        } while ((index = (index + 1) % TableSize) != hash % TableSize);
    }

    bool TrySet(String *key, T value)
    {
        std::shared_lock<std::shared_mutex> lck(ReadWriteMutex);

        HashMap *replacement = Replacement.load(std::memory_order_acquire);
        if (replacement)
        {
            return replacement->TrySet(key, value);
        }

        auto hash = key->GetHash();
        size_t index = hash % TableSize;
        Slot slot(key, value);

        do
        {
            std::atomic<Slot> &pos = Table()[index];
            Slot current = pos.load(std::memory_order_acquire);

            while (current.GetKey() == nullptr)
            {
                if (pos.compare_exchange_strong(current, slot))
                {
                    gc::Heap::GetInstance()->WriteBarrier(this, key);
                    gc::Heap::GetInstance()->WriteBarrier(this, ValueToRef(value));

                    KeyCount.fetch_add(1, std::memory_order_relaxed);

                    return true;
                }
            }

            while (current.GetKey()->EqualsTo(key))
            {
                if (pos.compare_exchange_strong(current, slot))
                {
                    gc::Heap::GetInstance()->WriteBarrier(this, key);
                    gc::Heap::GetInstance()->WriteBarrier(this, ValueToRef(value));

                    return true;
                }
            }
        } while ((index = (index + 1) % TableSize) != hash % TableSize);

        return false;
    }

    void SetAuto(gc::ThreadAllocator &allocator, String *key, T value)
    {
        HashMap *target = this;
        while (!target->TrySet(key, value))
        {
            target = target->Rehash(allocator);
        }
    }

    HashMap *Rehash(gc::ThreadAllocator &allocator)
    {
        auto replacement = Replacement.load();
        if (replacement)
        {
            return replacement;
        }

        std::unique_lock<std::shared_mutex> lck(ReadWriteMutex);

        replacement = Replacement.load();
        if (replacement)
        {
            return replacement;
        }

        replacement = NewOfLevel(allocator, KeyCount.load(std::memory_order_relaxed) < TableSize * 0.7 ? Level : (Level + 1));

        std::atomic<Slot> *table = Table();
        std::atomic<Slot> *limit = table + TableSize;

        for (auto s = table; s != limit; s++)
        {
            Slot slot = s->load();

            if (slot.IsDeleted())
            {
                continue;
            }

            if (slot.GetKey() == nullptr)
            {
                continue;
            }

            bool result = replacement->TrySet(slot.GetKey(), slot.Value);

            hydra_assert(result,
                "Set value must succeed");
        }

        auto original = Replacement.exchange(replacement);
        hydra_assert(original == nullptr,
            "No race condition would occur");

        return replacement;
    }

    bool Remove(String *key, T value)
    {
        std::shared_lock<std::shared_mutex> lck(ReadWriteMutex);

        HashMap *replacement = Replacement.load(std::memory_order_acquire);
        if (replacement)
        {
            return replacement->Remove(key, value);
        }

        auto hash = key->GetHash();
        size_t index = hash % TableSize;
        Slot slot(key, value);
        slot.SetDeleted();

        do
        {
            std::atomic<Slot> &pos = Table()[index];
            Slot current = pos.load(std::memory_order_acquire);

            if (current.GetKey() == nullptr)
            {
                return false;
            }

            if (current.GetKey()->EqualsTo(key))
            {
                if (current.IsDeleted())
                {
                    return false;
                }

                do
                {
                    if (pos.compare_exchange_strong(current, slot))
                    {
                        KeyCount.fetch_add(-1, std::memory_order_relaxed);

                        gc::Heap::GetInstance()->WriteBarrier(this, key);
                        gc::Heap::GetInstance()->WriteBarrier(this, ValueToRef(value));

                        return true;
                    }
                } while (current.GetKey()->EqualsTo(key) && current.Value == value);
            }
        } while ((index = (index + 1) % TableSize) != hash % TableSize);

        return false;
    }

private:
    struct Slot
    {
        uintptr_t Key;
        T Value;

        Slot()
            : Key(0), Value(T())
        { }

        Slot(String *key, T value)
            : Key(reinterpret_cast<uintptr_t>(key)), Value(value)
        { }

        String *GetKey()
        {
            return reinterpret_cast<String *>(Key & ~1ull);
        }

        bool IsDeleted()
        {
            return (Key & 1) != 0;
        }

        void SetDeleted()
        {
            Key |= 1;
        }
    };

    template <typename T>
    static std::enable_if_t<std::is_base_of<gc::HeapObject, typename std::remove_pointer<T>::type>::value>
        ScanValue(T value, std::function<void(gc::HeapObject *)> scan)
    {
        scan(value);
    }

    template <typename T>
    static auto ScanValue(T value, std::function<void(gc::HeapObject *)> scan)
        -> decltype(value.ScanValue(scan), void())
    {
        value.ScanValue(scan);
    }

    template <typename T>
    static auto ScanValue(T value, std::function<void(gc::HeapObject *)> scan)
        -> decltype(value->ScanValue(scan), void())
    {
        value->ScanValue(scan);
    }

    template <typename T>
    static std::enable_if_t<std::is_base_of<gc::HeapObject, typename std::remove_pointer<T>::type>::value, gc::HeapObject *>
        ValueToRef(T value)
    {
        return value;
    }

    template <typename T>
    static auto ValueToRef(T value)
        -> decltype(value.ToRef(), (gc::HeapObject*)(0))
    {
        return value.ToRef();
    }

    template <typename T>
    static auto ValueToRef(T value)
        -> decltype(value->ToRef(), (gc::HeapObject*)(0))
    {
        return value->ToRef();
    }

    std::atomic<Slot> *Table()
    {
        return reinterpret_cast<std::atomic<Slot> *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(HashMap));
    }

    std::shared_mutex ReadWriteMutex;
    std::atomic<HashMap *> Replacement;
    std::atomic<size_t> KeyCount;
    size_t Level;
    size_t TableSize;
};

} // namespace runtime
} // namespace hydra

#endif // _MANAGED_HASH_MAP_H_