#ifndef _COMPILED_FUNCTION_H_
#define _COMPILED_FUNCTION_H_

#include "Runtime/Type.h"

namespace hydra
{
namespace vm
{

class Scope;
struct IRFunc;

class CompiledFunction
{
public:
    virtual bool Call(
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

    inline size_t GetVarCount() const
    {
        return 0;
    }
};

class UncompiledFunction : public CompiledFunction
{
public:
    UncompiledFunction(IRFunc *ir)
        : IR(ir)
    { }

    virtual bool Call(
        gc::ThreadAllocator &allocator,
        Scope *scope,
        runtime::JSValue &retVal,
        runtime::JSValue &error);

private:
    IRFunc *IR;
};

} // namespace vm
} // namespace hydra

#endif // _COMPILED_FUNCITON_H_