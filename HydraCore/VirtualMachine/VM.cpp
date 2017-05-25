#include "VM.h"

#include "GarbageCollection/GC.h"
#include "Runtime/Semantic.h"

#include "Common/Singleton.h"
#include "Common/Platform.h"

#include "ByteCode.h"

#include <iostream>

namespace hydra
{
namespace vm
{

struct VMInitializeHelper : public Singleton<VMInitializeHelper>
{
    VMInitializeHelper()
    {
        gc::ThreadAllocator allocator(gc::Heap::GetInstance());

        runtime::semantic::SetOnGlobal(
            allocator,
            runtime::String::New(allocator, u"module"),
            runtime::JSValue::FromObject(runtime::semantic::NewNativeFunc(allocator, lib_Module)));

        runtime::semantic::SetOnGlobal(
            allocator,
            runtime::String::New(allocator, u"include"),
            runtime::JSValue::FromObject(runtime::semantic::NewNativeFunc(allocator, lib_Include)));
    }

    static bool lib_Module(gc::ThreadAllocator &allocator, runtime::JSValue thisArg, runtime::JSArray *arguments, runtime::JSValue &retVal, runtime::JSValue &error)
    {
        js_return(runtime::JSValue());
    }

    static bool lib_Include(gc::ThreadAllocator &allocator, runtime::JSValue thisArg, runtime::JSArray *arguments, runtime::JSValue &retVal, runtime::JSValue &error)
    {
        js_return(runtime::JSValue());
    }
};

VM::VM()
{
    VMInitializeHelper::GetInstance();

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

runtime::JSFunction *VM::Compile(gc::ThreadAllocator &allocator, const std::string &path)
{
    ByteCode byteCode(path);
    auto pair = Modules.emplace(byteCode.Load(allocator));
    hydra_assert(pair.second, "module is unique");

    auto moduleName = platform::NormalizePath({ path });

    auto local = runtime::semantic::MakeLocalGlobal(allocator, runtime::String::New(allocator, moduleName.begin(), moduleName.end()));

    return runtime::semantic::NewRootFunc(allocator, (*(pair.first))->Functions.front().get(), runtime::JSValue::FromObject(local));
}

void VM::CompileToTask(gc::ThreadAllocator &allocator, const std::string &path)
{
    AddTask(Compile(allocator, path));
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