#pragma once

#include "common/common.hpp"

namespace hydra
{

namespace internal
{
constexpr uint64_t UNDEFINED_CONTENT    = 0xFFFFFFFFFFFFFFFF;
constexpr uint64_t TRUE_CONTENT         = 0xFFFB000000000000;
constexpr uint64_t FALSE_CONTENT        = 0xFFFA000000000000;

constexpr uint64_t INTEGER_MARK         = 0xFFFC000000000000;
constexpr uint64_t SYMBOL_MASK          = 0xFFFD000000000000;

constexpr uint64_t STRING_MASK          = 0xFFF4000000000000;
constexpr uint64_t EXTERNAL_REF_MASK    = 0xFFF2000000000000;
constexpr uint64_t HEAP_REF_MASK        = 0xFFF6000000000000;

constexpr uint64_t FLOAT_EXPONENT_MASK  = 0x7FF0000000000000;
constexpr uint64_t FLOAT_VALID_VALUE    = 0x7FF0000000000000;

constexpr uint64_t NAN_VALID_MASK       = 0xFFF8000000000000;
constexpr uint64_t NAN_VALID_VALUE      = 0xFFF8000000000000;

constexpr uint64_t REF_VALID_MASK       = 0xFFF8000000000000;
constexpr uint64_t REF_VALID_VALUE      = 0xFFF0000000000000;

constexpr uint64_t INT_VALID_MASK       = 0xFFFF000000000000;
constexpr uint64_t INT_VALID_VALUE      = 0xFFFC000000000000;

constexpr uint64_t SYMBOL_VALID_MASK    = 0xFFFF000000000000;
constexpr uint64_t SYMBOL_VALID_VALUE   = 0xFFFD000000000000;

constexpr uint64_t BOOL_VALID_MASK      = 0xFFFF000000000000;
constexpr uint64_t BOOL_VALID_VALUE     = 0xFFFA000000000000;
}

struct Value
{
    Value()
        : Value(internal::UNDEFINED_CONTENT)
    { }

    inline bool IsDouble() const
    {
        return (internal::FLOAT_EXPONENT_MASK & _content) != internal::FLOAT_VALID_VALUE
            || (internal::NAN_VALID_MASK & _content) == internal::NAN_VALID_VALUE;
    }

    inline bool IsRef() const
    {
        return (internal::REF_VALID_MASK & _content) == internal::REF_VALID_VALUE;
    }

    inline bool IsInteger() const
    {
        return (internal::INT_VALID_MASK & _content) == internal::INT_VALID_VALUE;
    }

    inline bool IsUndefined() const
    {
        return (internal::UNDEFINED_CONTENT == _content);
    }

    inline bool IsSymbol() const
    {
        return (internal::SYMBOL_VALID_MASK & _content) == internal::SYMBOL_VALID_VALUE;
    }

    inline bool IsBool() const
    {
        return (internal::BOOL_VALID_MASK & _content) == internal::BOOL_VALID_VALUE;
    }

    static inline Value FromInt(int32_t value)
    {
        return Value(internal::INTEGER_MARK | static_cast<uint32_t>(value));
    }

    static inline Value FromDouble(double value)
    {
        return Value(*reinterpret_cast<uint64_t*>(&value));
    }

    static inline Value FromUndefined()
    {
        return Value();
    }

    static inline Value FromBool(bool value)
    {
        return Value(value ? internal::TRUE_CONTENT : internal::FALSE_CONTENT);
    }

    static inline Value FromSymbol(uint32_t value)
    {
        return Value(internal::SYMBOL_MASK | value);
    }

private:
    explicit Value (uint64_t content)
        : _content(content)
    { }

    uint64_t _content;
};

static_assert(sizeof(Value) == 8, "sizeof(Value) should be 8");

}