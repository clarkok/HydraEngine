#ifndef _COMPILE_H_
#define _COMPILE_H_

#include "xbyak/xbyak/xbyak.h"

#include "Scope.h"
#include "IR.h"
#include "IRInsts.h"

#include "Runtime/Type.h"
#include "GarbageCollection/GC.h"

namespace hydra
{
namespace vm
{

class CompileTask : public Xbyak::CodeGenerator
{
public:
    using Func = bool(*)(gc::ThreadAllocator &allocator,
        Scope *scope,
        runtime::JSValue &retVal,
        runtime::JSValue &error);

    using Label = Xbyak::Label;

    CompileTask()
        : Xbyak::CodeGenerator(4096, Xbyak::AutoGrow)
    { }

    virtual Func Compile() = 0;

protected:
    Func GetCode()
    {
        ready();
        return getCode<Func>();
    }
};

class BaselineCompileTask : public CompileTask
{
public:
    BaselineCompileTask(IRFunc *ir)
        : CompileTask(), IR(ir)
    { }

    virtual Func Compile() override final;

private:
    IRFunc *IR;
};

} // namespace vm
} // namespace hydra

#endif // _COMPILE_H_