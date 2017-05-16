#ifndef _IR_H_
#define _IR_H_

#include "Common/HydraCore.h"
#include "Runtime/String.h"
#include "Runtime/JSArray.h"

#include "Replacable.h"

#include "CompiledFunction.h"

#include <list>
#include <vector>
#include <memory>

namespace hydra
{
namespace vm
{

struct IRInst : public Replacable<struct IRInst>
{
    virtual ~IRInst() = default;

    virtual size_t GetType() const = 0;

    template <typename T>
    T *As()
    {
        static_assert(std::is_base_of<IRInst, T>::value,
            "T must inherit from IRInst");

        return dynamic_cast<T*>(this);
    }

    size_t Index;
};

struct IRBlock : public Replacable<struct IRBlock>
{
    size_t Index;
    std::list<std::unique_ptr<IRInst>> Insts;
    IRInst::Ref Condition;
    IRBlock::Ref Consequent;
    IRBlock::Ref Alternate;
};

struct IRModule;

struct IRFunc
{
    IRFunc(IRModule *module, runtime::String *name, size_t length)
        : Module(module), Name(name), Length(length)
    { }

    std::list<std::unique_ptr<IRBlock>> Blocks;
    IRModule *Module;
    runtime::String *Name;
    size_t Length;
    size_t VarCount;

    std::unique_ptr<CompiledFunction> CompiledFunction;

    inline void UpdateIndex()
    {
        size_t blockIndex = 0;
        size_t instIndex = 0;
        for (auto &block : Blocks)
        {
            block->Index = blockIndex++;
            for (auto &inst : block->Insts)
            {
                inst->Index = instIndex++;
            }
        }
    }
};

struct IRModule
{
    IRModule(runtime::JSArray *stringsReferenced);
    IRModule(const IRModule &) = delete;
    IRModule(IRModule &&) = delete;
    IRModule &operator = (const IRModule &) = delete;
    IRModule &operator = (IRModule &&) = delete;

    ~IRModule();

    std::vector<std::unique_ptr<IRFunc>> Functions;
    runtime::JSArray *StringsReferenced;
};

} // namespace vm
} // namespace hydra

#endif // _IR_H_