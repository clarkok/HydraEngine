#ifndef _VM_H_
#define _VM_H_

#include "Common/HydraCore.h"
#include "Common/Singleton.h"
#include "Runtime/JSFunction.h"

#include "IR.h"
#include "VMDefs.h"

#include <queue>
#include <set>
#include <string>
#include <memory>

namespace hydra
{
namespace vm
{

class VM : public Singleton<VM>
{
public:
    VM();

    int Execute(gc::ThreadAllocator &allocator);

    runtime::JSFunction *Compile(gc::ThreadAllocator &allocator, const std::string &path);
    void CompileToTask(gc::ThreadAllocator &allocator, const std::string &path);
    void AddTask(runtime::JSFunction *);
    void Stop();

    void LoadJsLib(gc::ThreadAllocator &allocator);

private:
    std::queue<runtime::JSFunction *> Queue;
    std::multiset<runtime::JSFunction *> References;
    std::set<std::unique_ptr<IRModule>> Modules;
};

} // namespace vm
} // namespace hydra

#endif // _VM_H_