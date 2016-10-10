#pragma once

#include <cassert>

namespace hydra
{

template <typename T>
struct Ptr
{
    T *Value;

    Ptr(T* value = nullptr)
        : Value(value)
    { }
    Ptr(const Ptr &) = default;
    Ptr(Ptr &&) = default;

    Ptr & operator = (const Ptr &) = default;
    Ptr & operator = (Ptr &&) = default;

    bool operator < (Ptr b) const
    {
        return Value < b.Value;
    }

    bool operator <= (Ptr b) const
    {
        return Value <= b.Value;
    }

    bool operator > (Ptr b) const
    {
        return Value > b.Value;
    }

    bool operator >= (Ptr b) const
    {
        return Value >= b.Value;
    }

    bool operator == (Ptr b) const
    {
        return Value == b.Value;
    }

    bool operator != (Ptr b) const
    {
        return Value != b.Value;
    }

    T &operator *() const
    {
        assert(Value);
        return *Value;
    }

    T *operator ->() const
    {
        assert(Value);
        return Value;
    }

    template <typename U>
    U *To() const
    {
        return reinterpret_cast<U*>(Value);
    }

    template <typename U>
    U *Cast() const
    {
        return dynamic_cast<U*>(Value);
    }
};

}
