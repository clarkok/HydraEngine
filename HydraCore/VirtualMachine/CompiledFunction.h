#ifndef _COMPILED_FUNCTION_H_
#define _COMPILED_FUNCTION_H_

#include "Runtime/Type.h"

#include "Compile.h"

#include "VMDefs.h"

#include <memory>

namespace hydra
{
namespace vm
{

class Scope;
struct IRFunc;

class CompiledFunction
{
public:
    CompiledFunction(std::unique_ptr<CompileTask> &&task, size_t length, size_t varCount)
        : Task(std::move(task)), Func(nullptr), Length(length), VarCount(varCount)
    {
        Func = Task->Get(RegisterCount);
    }

    inline bool Call(
        gc::ThreadAllocator &allocator,
        Scope *scope,
        runtime::JSValue &retVal,
        runtime::JSValue &error)
    {
        return Func(allocator, scope, retVal, error);
    }

    inline size_t GetRegisterCount() const
    {
        return RegisterCount;
    }

    inline size_t GetLength() const
    {
        return Length;
    }

    inline size_t GetVarCount() const
    {
        return VarCount;
    }

protected:
    std::unique_ptr<CompileTask> Task;
    GeneratedCode Func;
    size_t RegisterCount;
    size_t Length;
    size_t VarCount;
};

} // namespace vm
} // namespace hydra

#endif // _COMPILED_FUNCITON_H_