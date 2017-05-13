#ifndef _COMPILED_FUNCTION_H_
#define _COMPILED_FUNCITON_H_

#include "Runtime/Type.h"

namespace hydra
{
namespace vm
{

class Scope;

class CompiledFunction
{
public:
    bool Call(
        gc::ThreadAllocator &allocator,
        Scope *scope,
        runtime::JSValue &retVal,
        runtime::JSValue &error);

    inline size_t GetRegisterCount() const
    {
        return 0;
    }

    inline size_t GetLength() const
    {
        return 0;
    }
};

} // namespace vm
} // namespace hydra

#endif // _COMPILED_FUNCITON_H_