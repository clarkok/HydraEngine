#pragma once

#include "common/ptr.hpp"

#include <cstdint>

namespace hydra
{

// A class stands for a reference to a heap object
template <typename T>
struct Ref : Ptr<T>
{
    Ref(T *value = nullptr)
        : Ptr(value)
    {
        assert(static_cast<std::uint64_t>(value) & 7 == 0);
    }
};

}