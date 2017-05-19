#ifndef _CONCURRENT_LEVEL_HASH_SET_H_
#define _CONCURRENT_LEVEL_HASH_SET_H_

#include "HydraCore.h"
#include "Platform.h"
#include "Constexpr.h"

#include <array>
#include <algorithm>

namespace hydra
{
namespace concurrent
{

/* LevelHashSet never shrink */
template <typename T,
    typename T_Hash = u64,
    size_t LevelSize = 512,
    size_t LevelHashWidth = cexpr::LSBShift(LevelSize),
    size_t HashWidth = cexpr::TypeBitCount<T_Hash>::value,
    size_t LevelCount = (HashWidth + LevelHashWidth - 1) / LevelHashWidth
>
class LevelHashSet
{
public:
    template<typename T_Ptr>
    inline static auto Hash(T_Ptr ptr) -> decltype(ptr->Hash(), T_Hash())
    {
        return ptr->Hash();
    }

    template<typename T_Ptr>
    inline static auto Hash(T_Ptr ptr) -> decltype(hash(ptr), T_Hash())
    {
        return hash(ptr);
    }

    LevelHashSet() = default;

    ~LevelHashSet() = default;

    bool Add(T *ptr)
    {
        hydra_assert(ptr != nullptr,
            "nullptr not supported");

        T_Hash hash = Hash(ptr);
        return RootLevel.Add(ptr, hash);
    }

    bool Remove(T *ptr)
    {
        hydra_assert(ptr != nullptr,
            "nullptr not supported");

        T_Hash hash = Hash(ptr);
        return RootLevel.Remove(ptr, hash);
    }

    bool Has(T* ptr)
    {
        hydra_assert(ptr != nullptr,
            "nullptr not supported");

        T_Hash hash = Hash(ptr);
        return RootLevel.Has(ptr, hash);
    }

private:
    static_assert((LevelSize & (LevelSize - 1)) == 0,
        "LevelSize must be power of 2");

    template <size_t Level, typename isEnable = void>
    struct LevelTable
    { };

    template <size_t Level>
    struct LevelTable<Level, std::enable_if_t<Level < LevelCount - 1> >
    {
        std::array<std::atomic<uintptr_t>, LevelSize> Table;

        LevelTable()
        {
            std::fill(Table.begin(), Table.end(), 0);
        }

        ~LevelTable()
        {
            for (auto &slot : Table)
            {
                auto slotValue = slot.load();
                if (IsNextLevel(slotValue))
                {
                    delete CastToLevel<Level + 1>(slotValue);
                }
            }
        }

        constexpr T_Hash GetLevelHash(T_Hash hash)
        {
            return cexpr::SubBits(hash, LevelHashWidth * Level, LevelHashWidth);
        }

        bool Add(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            uintptr_t slotValue = Table[levelHash].load(std::memory_order_relaxed);

            for (;;)
            {
                if (IsNextLevel(slotValue))
                {
                    return CastToLevel<Level + 1>(slotValue)->Add(ptr, hash);
                }
                else if (slotValue)
                {
                    auto *nextLevel = new LevelTable<Level + 1>();
                    T* original = CastToT(slotValue);
                    if (original == ptr)
                    {
                        return true;
                    }

                    bool originalResult = nextLevel->Add(original, Hash(original));
                    hydra_assert(!originalResult,
                        "nextLevel should be empty");
                    nextLevel->Add(ptr, hash);

                    if (Table[levelHash].compare_exchange_strong(slotValue, CastFromLevel(nextLevel)))
                    {
                        return false;
                    }

                    delete nextLevel;
                }
                else
                {
                    if (Table[levelHash].compare_exchange_strong(slotValue, CastFromT(ptr)))
                    {
                        return false;
                    }
                }
            }
        }

        bool Remove(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            uintptr_t slotValue = Table[levelHash].load(std::memory_order_relaxed);

            for (;;)
            {
                if (IsNextLevel(slotValue))
                {
                    return CastToLevel<Level + 1>(slotValue)->Remove(ptr, hash);
                }
                else
                {
                    if (CastToT(slotValue) != ptr)
                    {
                        return false;
                    }

                    if (Table[levelHash].compare_exchange_strong(slotValue, 0))
                    {
                        return true;
                    }
                }
            }
        }

        bool Has(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            uintptr_t slotValue = Table[levelHash].load(std::memory_order_consume);

            if (IsNextLevel(slotValue))
            {
                return CastToLevel<Level + 1>(slotValue)->Has(ptr, hash);
            }

            return CastToT(slotValue) == ptr;
        }
    };

    template <size_t Level>
    struct LevelTable<Level, std::enable_if_t<Level == LevelCount - 1> >
    {
        std::array<std::atomic<T *>, LevelSize> Table;

        LevelTable()
        {
            std::fill(Table.begin(), Table.end(), nullptr);
        }

        ~LevelTable() = default;

        constexpr T_Hash GetLevelHash(T_Hash hash)
        {
            return cexpr::SubBits(hash, HashWidth - LevelHashWidth, LevelHashWidth);
        }

        bool Add(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            T *original = Table[levelHash].exchange(ptr);

            hydra_assert(!original || original == ptr,
                "Hash in LevelHashSet should be unique!");

            return original == nullptr;
        }

        bool Remove(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            T *original = Table[levelHash].exchange(nullptr);

            hydra_assert(!original || original == ptr,
                "Hash in LevelHashSet should be unique!");

            return original == ptr;
        }

        bool Has(T *ptr, T_Hash hash)
        {
            auto levelHash = GetLevelHash(hash);
            return Table[levelHash].load(std::memory_order_consume) == ptr;
        }
    };

    static constexpr bool IsNextLevel(uintptr_t value)
    {
        return (value & 1) != 0;
    }

    template <size_t Level>
    static constexpr LevelTable<Level> *CastToLevel(uintptr_t value)
    {
        return reinterpret_cast<LevelTable<Level> *>(value & ~1);
    }

    template <size_t Level>
    static constexpr uintptr_t CastFromLevel(LevelTable<Level> *level)
    {
        return reinterpret_cast<uintptr_t>(level) | 1;
    }

    static constexpr T *CastToT(uintptr_t value)
    {
        return reinterpret_cast<T *>(value);
    }

    static constexpr uintptr_t CastFromT(T *ptr)
    {
        return reinterpret_cast<uintptr_t>(ptr);
    }

    LevelTable<0> RootLevel;
};

}
}

#endif // _CONCURRENT_LEVEL_HASH_SET_H_