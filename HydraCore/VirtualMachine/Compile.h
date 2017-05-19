#ifndef _COMPILE_H_
#define _COMPILE_H_

#include "Scope.h"

#include "Runtime/Type.h"
#include "GarbageCollection/GC.h"

#include "VMDefs.h"

#include "xbyak/xbyak/xbyak.h"

namespace hydra
{
namespace vm
{

struct IRFunc;

class CompileTask : public Xbyak::CodeGenerator
{
public:
    using Label = Xbyak::Label;

    CompileTask()
        : Xbyak::CodeGenerator(4096, Xbyak::AutoGrow), Generated(nullptr)
    { }

    virtual GeneratedCode Compile(size_t &registerCount) = 0;

    inline GeneratedCode Get()
    {
        if (!Generated)
        {
            Generated = Compile(RegisterCount);
        }
        return Generated;
    }

    inline GeneratedCode Get(size_t &registerCount)
    {
        if (!Generated)
        {
            Generated = Compile(RegisterCount);
        }
        registerCount = RegisterCount;
        return Generated;
    }

protected:
    GeneratedCode GetCode()
    {
        ready();
        return getCode<GeneratedCode>();
    }

    GeneratedCode Generated;
    size_t RegisterCount;
};

class BaselineCompileTask : public CompileTask
{
public:
    BaselineCompileTask(IRFunc *ir)
        : CompileTask(), IR(ir)
    { }

    virtual GeneratedCode Compile(size_t &registerCount) override final;

private:
    IRFunc *IR;
};

} // namespace vm
} // namespace hydra

#endif // _COMPILE_H_