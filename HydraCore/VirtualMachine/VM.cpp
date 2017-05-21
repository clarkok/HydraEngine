#include "VM.h"

#include "GarbageCollection/GC.h"
#include "Runtime/Semantic.h"

#include "ByteCode.h"

#include <iostream>

namespace hydra
{
namespace vm
{

VM::VM()
{
    gc::Heap::GetInstance()->RegisterRootScanFunc([this](std::function<void(gc::HeapObject *)> scan)
    {
        for (auto &task : References)
        {
            scan(task);
        }
    });
}

int VM::Execute(gc::ThreadAllocator &allocator)
{
    // TODO
    while (!Queue.empty())
    {
        auto task = Queue.front(); Queue.pop();
        auto iter = References.find(task);

        hydra_assert(iter != References.end(),
            "there must be a reference");

        References.erase(iter);

        auto global = runtime::semantic::GetGlobalObject();
        auto arguments = runtime::semantic::NewArrayInternal(allocator);

        runtime::JSValue retVal;
        runtime::JSValue error;

        auto result = task->Call(allocator, runtime::JSValue::FromObject(global), arguments, retVal, error);
        if (!result)
        {
            std::cout << "Error thrown" << std::endl;
        }
    }

    return 0;
}

void VM::CompileToTask(gc::ThreadAllocator &allocator, const std::string &path)
{
    ByteCode byteCode(path);
    auto pair = Modules.emplace(byteCode.Load(allocator));
    hydra_assert(pair.second, "module is unique");

    runtime::JSCompiledFunction *task = runtime::semantic::NewRootFunc(allocator, (*(pair.first))->Functions.front().get());
    AddTask(task);
}

void VM::AddTask(runtime::JSFunction *func)
{
    References.insert(func);
    Queue.push(func);
}

void VM::Stop()
{
    Logger::GetInstance()->Shutdown();
    gc::Heap::GetInstance()->Shutdown();
}

} // namespace vm
} // namespace hydra