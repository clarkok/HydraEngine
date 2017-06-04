#ifndef _IR_H_
#define _IR_H_

#include "Common/HydraCore.h"
#include "Runtime/String.h"
#include "Runtime/JSArray.h"

#include "Replacable.h"

#include "CompiledFunction.h"

#include "Common/Singleton.h"

#include <list>
#include <set>
#include <vector>
#include <memory>
#include <future>
#include <ostream>

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

    template <typename T>
    bool Is()
    {
        return As<T>() != nullptr;
    }

    size_t InstIndex;

    virtual void Dump(std::ostream &os) = 0;
};

struct IRBlock : public Replacable<struct IRBlock>
{
    size_t Index;
    std::list<std::unique_ptr<IRInst>> Insts;
    IRInst::Ref Condition;
    IRBlock::Ref Consequent;
    IRBlock::Ref Alternate;

    IRBlock::Ref Dominator;
    std::list<IRBlock::Ref> Precedences;

    IRInst::Ref Scope;
    IRInst::Ref EndScope;

    IRBlock::Ref LoopHeader;
    IRBlock::Ref LoopPrev;
    size_t LoopDepth;

    void Dump(std::ostream &os);
};

struct IRModule;

struct IRFunc
{
    IRFunc(IRModule *module, runtime::String *name, size_t length)
        : Module(module),
        Name(name),
        Length(length),
        Compiled(nullptr),
        BaselineFunction(nullptr),
        OptimizedFunction(nullptr)
    { }

    std::list<std::unique_ptr<IRBlock>> Blocks;
    IRModule *Module;
    runtime::String *Name;
    size_t Length;

    std::atomic<CompiledFunction *> Compiled;

    std::shared_future<CompiledFunction *> BaselineFuture;

    std::unique_ptr<CompiledFunction> BaselineFunction;
    std::unique_ptr<CompiledFunction> OptimizedFunction;

    inline size_t UpdateIndex()
    {
        size_t blockIndex = 0;
        size_t instIndex = 0;
        for (auto &block : Blocks)
        {
            block->Index = blockIndex++;
            for (auto &inst : block->Insts)
            {
                inst->InstIndex = instIndex++;
            }
        }
        return instIndex;
    }

    size_t GetVarCount() const;

    void Dump(std::ostream &os);
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

class IRModuleGCHelper : public Singleton<IRModuleGCHelper>
{
public:
    IRModuleGCHelper()
    {
        gc::Heap::GetInstance()->RegisterRootScanFunc(RootScan);
    }

    inline void AddModule(IRModule *mod)
    {
        std::unique_lock<std::mutex> lck(ModulesMutex);
        Modules.insert(mod);
    }

    inline void RemoveModule(IRModule *mod)
    {
        std::unique_lock<std::mutex> lck(ModulesMutex);
        Modules.erase(mod);
    }

private:
    std::set<IRModule *> Modules;
    std::mutex ModulesMutex;

    static void RootScan(std::function<void(gc::HeapObject*)> scan)
    {
        auto instance = GetInstance();

        std::unique_lock<std::mutex> lck(instance->ModulesMutex);
        for (auto irModule : instance->Modules)
        {
            scan(irModule->StringsReferenced);
        }
    }
};

} // namespace vm
} // namespace hydra

#endif // _IR_H_