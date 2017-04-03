#ifndef _CONSTEXPR_H_
#define _CONSTEXPR_H_

#include "HydraCore.h"

#include <type_traits>

namespace hydra
{
namespace cexpr
{

constexpr bool IsPowerOf2(size_t value)
{
    return (value & (value - 1)) == 0;
}

template <typename T>
struct TypeBitCount
{
    static constexpr size_t value = sizeof(T) * 8;
};

template <size_t bitcount>
struct UnsignedIntegerType
{
};

template <>
struct UnsignedIntegerType<8>
{
    typedef u8 Type;
};

template <>
struct UnsignedIntegerType<16>
{
    typedef u16 Type;
};

template <>
struct UnsignedIntegerType<32>
{
    typedef u32 Type;
};

template <>
struct UnsignedIntegerType<64>
{
    typedef u64 Type;
};

template <typename T_to,
    typename T_from,
    T_from pattern,
    typename Enable = void>
struct BitRep
{
};

template <typename T_to,
    typename T_from,
    T_from pattern>
struct BitRep<
    T_to,
    T_from,
    pattern,
    std::enable_if_t<sizeof(T_from) == sizeof(T_to)>>
{
    static constexpr T_to value = pattern;
};

template <typename T_to,
    typename T_from,
    T_from pattern>
struct BitRep<
    T_to,
    T_from,
    pattern,
    std::enable_if_t<sizeof(T_from) * 2 == sizeof(T_to)>>
{
    static constexpr T_to value = ((T_to)pattern) |
        (((T_to)pattern) << TypeBitCount<T_from>::value);
};

template <typename T_to,
    typename T_from,
    T_from pattern>
struct BitRep<
    T_to,
    T_from,
    pattern,
    std::enable_if_t<sizeof(T_from) * 4 == sizeof(T_to)>>
{
    static constexpr T_to value = ((T_to)pattern) |
        (((T_to)pattern) << TypeBitCount<T_from>::value) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 2)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 3));
};

template <typename T_to,
    typename T_from,
    T_from pattern>
struct BitRep<
    T_to,
    T_from,
    pattern,
    std::enable_if_t<sizeof(T_from) * 8 == sizeof(T_to)>>
{
    static constexpr T_to value = ((T_to)pattern) |
        (((T_to)pattern) << TypeBitCount<T_from>::value) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 2)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 3)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 4)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 5)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 6)) |
        (((T_to)pattern) << (TypeBitCount<T_from>::value * 7));
};

constexpr size_t BitCount(u64 value)
{
    value = (value & BitRep<u64, u8, 0x55>::value) + ((value & BitRep<u64, u8, 0xAA>::value) >> 1);
    value = (value & BitRep<u64, u8, 0x33>::value) + ((value & BitRep<u64, u8, 0xCC>::value) >> 2);
    value = (value & BitRep<u64, u8, 0x0F>::value) + ((value & BitRep<u64, u8, 0xF0>::value) >> 4);
    value = (value & BitRep<u64, u16, 0x00FF>::value) + ((value & BitRep<u64, u16, 0xFF00>::value) >> 8);
    value = (value & BitRep<u64, u32, 0x0000FFFF>::value) + ((value & BitRep<u64, u32, 0xFFFF0000>::value) >> 16);
    value = (value & 0xFFFFFFFF) + (value >> 32);

    return value;
}

constexpr size_t LSB(u64 value)
{
    return value & ((~value) + 1);
}

constexpr size_t LSBShift(u64 value)
{
    return BitCount(LSB(value) - 1);
}

constexpr u64 Mask(size_t offset, size_t width)
{
    return ((1ull << width) - 1) << offset;
}

constexpr u64 SubBits(u64 value, size_t offset, size_t width)
{
    return (value & Mask(offset, width)) >> offset;
}

constexpr u64 SignExt(u64 value, size_t signOffset)
{
    return (value & Mask(signOffset, 1))
        ? (value | Mask(signOffset + 1, 63 - signOffset))
        : value;
}

}
}

#endif // _CONSTEXPR_H_