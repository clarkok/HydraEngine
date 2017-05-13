#ifndef _IR_H_
#define _IR_H_

#include "Common/HydraCore.h"
#include "Runtime/String.h"
#include "Runtime/JSArray.h"

#include "Replacable.h"

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
};

struct IRBlock : public Replacable<struct IRBlock>
{
    IRBlock() = default;

    std::list<std::unique_ptr<IRInst>> Insts;
    IRInst::Ref Condition;
    IRBlock::Ref Consequent;
    IRBlock::Ref Alternate;
};

struct IRFunc
{
    IRFunc(runtime::String *name, size_t length)
        : Name(name), Length(length)
    { }

    std::list<std::unique_ptr<IRBlock>> Blocks;
    runtime::String *Name;
    size_t Length;
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