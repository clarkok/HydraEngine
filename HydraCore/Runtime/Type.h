#ifndef _TYPE_H_
#define _TYPE_H_

#include "Common/HydraCore.h"
#include "Common/Constexpr.h"

#include "GarbageCollection/GC.h"

#include <cmath>

namespace hydra
{
namespace runtime
{

class String;
class JSObject;

enum class Type
{
                    // 63-----56 55-----48 47-----40 39-----32 31-----24 23-----16 15------8 7-------0
    T_UNDEFINED,    // 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 
    T_NOT_EXISTS,   // 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1111 1110 
    T_BOOLEAN,      // 1111 1111 1111 1001 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX

    T_SMALL_INT,    // 1111 1111 1111 1000 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX

    T_NUMBER,       // XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
        // NaN         1111 1111 1111 0100 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000
    T_SYMBOL,       // 1111 1111 1111 1100 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
    T_STRING,       // 1111 1111 1111 1011 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
    T_OBJECT,       // 1111 1111 1111 1010 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX
    T_VALREF,       // 1111 1111 1111 1101 XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX

    T_VARIANT,      // 1111 1111 1111 ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    T_UNKNOWN
};

struct JSValue
{
    u64 Payload;

    static constexpr u64 NAN_PAYLOAD = 0xFFF4'0000'0000'0000ull;
    static constexpr u64 UNDEFINED_PAYLOAD = 0xFFFF'FFFF'FFFF'FFFFull;
    static constexpr u64 NOT_EXISTS_PAYLOAD = 0xFFFF'FFFF'FFFF'FFFEull;

    JSValue()
        : JSValue(NOT_EXISTS_PAYLOAD)
    { }

    explicit JSValue(u64 payload)
        : Payload(payload)
    { }

    static const JSValue &AtAddress(void *addr)
    {
        return *reinterpret_cast<JSValue *>(addr);
    }

    inline static Type GetType(JSValue value)
    {
        if (!std::isnan(value.Number()))
        {
            return Type::T_NUMBER;
        }
        else if (value.Payload == NAN_PAYLOAD)
        {
            return Type::T_NUMBER;
        }

        auto flag = cexpr::SubBits(value.Payload, 48, 4);

        if (!cexpr::SubBits(flag, 3, 1))
        {
            return Type::T_UNKNOWN;
        }

        const static runtime::Type LOOKUP_TABLE[] = {
            Type::T_SMALL_INT,
            Type::T_BOOLEAN,
            Type::T_OBJECT,
            Type::T_STRING,
            Type::T_SYMBOL,
            Type::T_VALREF,
            Type::T_UNKNOWN,
            Type::T_UNDEFINED
        };

        return LOOKUP_TABLE[cexpr::SubBits(flag, 0, 3)];
    }

    inline Type Type() const
    {
        return GetType(*this);
    }

    inline u64 GetLast48Bit() const
    {
        return cexpr::SubBits(Payload, 0, 48);
    }

    inline i64 GetLast48BitSign() const
    {
        return cexpr::SignExt(GetLast48Bit(), 47);
    }

    static inline JSValue FromLast48Bit(runtime::Type type, u64 value)
    {
        constexpr static u16 LOOKUP_TABLE[] = {
            0,
            0xFFFF,
            0xFFF9,
            0xFFF8,
            0,
            0xFFFC,
            0xFFFB,
            0xFFFA
        };

        return JSValue(
            static_cast<u64>(LOOKUP_TABLE[static_cast<int>(type)]) << 48 |
            cexpr::SubBits(value, 0, 48)
        );
    }

    inline static JSValue Undefined()
    {
        return JSValue(UNDEFINED_PAYLOAD);
    }

    inline double Number() const
    {
        return *reinterpret_cast<const double *>(&Payload);
    }

    inline static JSValue FromNumber(double value)
    {
        if (std::isnan(value))
        {
            return JSValue(NAN_PAYLOAD);
        }
        return JSValue(*reinterpret_cast<const u64 *>(&value));
    }

    inline bool Boolean() const
    {
        return !!GetLast48Bit();
    }

    inline static JSValue FromBoolean(u64 value)
    {
        return FromLast48Bit(Type::T_BOOLEAN, value);
    }

    inline i64 SmallInt() const
    {
        return GetLast48BitSign();
    }

    inline static JSValue FromSmallInt(i64 value)
    {
        return FromLast48Bit(Type::T_SMALL_INT, value);
    }

    inline u64 Symbol() const
    {
        return GetLast48Bit();
    }

    inline static JSValue FromSymbol(u64 value)
    {
        return FromLast48Bit(Type::T_SYMBOL, value);
    }

    String *String() const;

    inline static JSValue FromString(runtime::String *value)
    {
        hydra_assert(reinterpret_cast<uintptr_t>(value) <= cexpr::Mask(0, 48),
            "String pointer must be in 48bits");

        return FromLast48Bit(Type::T_STRING, reinterpret_cast<uintptr_t>(value));
    }

    JSObject *Object() const;

    inline static JSValue FromObject(JSObject *value)
    {
        hydra_assert(reinterpret_cast<uintptr_t>(value) <= cexpr::Mask(0, 48),
            "JSObject pointer must be in 48bits");

        return FromLast48Bit(Type::T_OBJECT, reinterpret_cast<uintptr_t>(value));
    }

    inline gc::HeapObject *ToReference() const
    {
        hydra_assert(IsReference(), "Only a reference value can be convert to reference");
        return reinterpret_cast<gc::HeapObject *>(GetLast48Bit());
    }

    inline bool IsReference() const
    {
        auto type = Type();
        return type == Type::T_STRING || type == Type::T_OBJECT;
    }

    inline bool IsUndefined() const
    {
        auto type = Type();
        return type == Type::T_UNDEFINED || type == Type::T_NOT_EXISTS;
    }

    inline JSValue *VarRef()
    {
        return reinterpret_cast<runtime::JSValue *>(GetLast48Bit());
    }

    inline static JSValue FromVarRef(JSValue *ref)
    {
        hydra_assert(reinterpret_cast<uintptr_t>(ref) <= cexpr::Mask(0, 48),
            "JSValue point must be in 48bits");

        return FromLast48Bit(Type::T_VALREF, reinterpret_cast<uintptr_t>(ref));
    }

    inline void ScanValue(std::function<void(gc::HeapObject*)> scan) const
    {
        if (IsReference())
        {
            scan(ToReference());
        }
    }

    inline bool operator == (JSValue other) const
    {
        return Payload == other.Payload;
    }

    inline bool operator != (JSValue other) const
    {
        return !operator == (other);
    }
};

static_assert(sizeof(JSValue) == sizeof(u64),
    "Sizeof JSValue should match with sizeof u64");

} // namepsace runtime
} // namepsace hydra

#endif // _TYPE_H_