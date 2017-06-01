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
    using JSValue = runtime::JSValue;
    using JSArray = runtime::JSArray;
    using String = runtime::String;
    using Type = runtime::Type;

    VMInitializeHelper()
    {
        gc::ThreadAllocator allocator(gc::Heap::GetInstance());

        runtime::semantic::SetOnGlobal(
            allocator,
            String::New(allocator, u"__execute"),
            JSValue::FromObject(runtime::semantic::NewNativeFunc(allocator, lib_Execute)));

        runtime::semantic::SetOnGlobal(
            allocator,
            String::New(allocator, u"__normalize_path"),
            JSValue::FromObject(runtime::semantic::NewNativeFunc(allocator, lib_NormalizePath)));
    }

    static bool lib_Execute(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
    {
        JSValue normalized;
        if (!lib_NormalizePath(allocator, thisArg, arguments, normalized, error))
        {
            return false;
        }

        auto func = VM::GetInstance()->Compile(allocator, normalized.String()->ToString());
        auto args = runtime::semantic::NewArrayInternal(allocator);
        return func->Call(allocator, thisArg, args, retVal, error);
    }

    static bool lib_NormalizePath(gc::ThreadAllocator &allocator, JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
    {
        std::vector<std::string> paths;
        for (size_t i = 0; i < arguments->GetLength(); ++i)
        {
            JSValue arg;
            if (!runtime::semantic::ObjectGetSafeArray(allocator, arguments, i, arg, error))
            {
                return false;
            }

            JSValue str;
            if (!runtime::semantic::ToString(allocator, arg, str, error))
            {
                return false;
            }

            paths.push_back(str.String()->ToString());
        }

        std::string normalized = platform::NormalizePath(paths.begin(), paths.end());

        js_return(JSValue::FromString(String::New(allocator, normalized.begin(), normalized.end())));
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

    for (auto &funcIter : pair.first->get()->Functions)
    {
        auto func = funcIter.get();
        func->BaselineFuture = std::move(std::async(
            std::launch::deferred,
            [func]() -> CompiledFunction *
            {
                std::unique_ptr<CompileTask> task(new BaselineCompileTask(func));

                size_t registerCount;
                func->BaselineFunction = std::make_unique<CompiledFunction>(
                    std::move(task),
                    func->Length,
                    func->GetVarCount());

                CompiledFunction *current = nullptr;
                func->Compiled.compare_exchange_strong(current, func->BaselineFunction.get());

                return func->BaselineFunction.get();
            }
        ));
    }

    auto moduleName = platform::NormalizePath({ path });
    auto moduleDir = platform::GetDirectoryOfPath(moduleName);

    auto local = runtime::semantic::MakeLocalGlobal(
        allocator,
        runtime::String::New(allocator, moduleName.begin(), moduleName.end()),
        runtime::String::New(allocator, moduleDir.begin(), moduleDir.end()));

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

void VM::LoadJsLib(gc::ThreadAllocator &allocator)
{
    /*
    auto libInit = Compile(allocator,
        platform::NormalizePath({
            platform::GetDirectoryOfPath(__FILE__),
            "../../HydraJsLib/index.ir"
        }));
        */
    auto libInit = Compile(allocator,
        "./index.ir");
    auto args = runtime::semantic::NewArrayInternal(allocator);

    runtime::JSValue retVal;
    runtime::JSValue error;
    auto result = libInit->Call(allocator, runtime::JSValue(), args, retVal, error);

    hydra_assert(result, "Failed to load JsLib");
}

} // namespace vm
} // namespace hydra