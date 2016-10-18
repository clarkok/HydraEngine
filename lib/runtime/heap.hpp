#pragma once

#include "value.hpp"

namespace hydra
{

template<typename T>
class Handle
{
    Value Ref;
public:
    friend class Heap;
};

class Heap
{
public:

protected:
    virtual void *Allocate(size_t) = 0;
    virtual void Release(void *) = 0;
};

}