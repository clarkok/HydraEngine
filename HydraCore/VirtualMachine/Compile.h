#ifndef _COMPILE_H_
#define _COMPILE_H_

#include "Scope.h"
#include "IR.h"
#include "IRInsts.h"

#include "Runtime/Type.h"
#include "GarbageCollection/GC.h"

#include "VMDefs.h"

#include "xbyak/xbyak/xbyak.h"

namespace hydra
{
namespace vm
{

class CompileTask : public Xbyak::CodeGenerator
{
public:
    using Label = Xbyak::Label;

    CompileTask()
        : Xbyak::CodeGenerator(4096, Xbyak::AutoGrow)
    { }

    virtual GeneratedCode Compile() = 0;

protected:
    GeneratedCode GetCode()
    {
        ready();
        return getCode<GeneratedCode>();
    }
};

class BaselineCompileTask : public CompileTask
{
public:
    BaselineCompileTask(IRFunc *ir)
        : CompileTask(), IR(ir)
    { }

    virtual GeneratedCode Compile() override final;

private:
    IRFunc *IR;
};

} // namespace vm
} // namespace hydra

#endif // _COMPILE_H_