#include "IR.h"

#include "Common/Singleton.h"
#include "GarbageCollection/GC.h"

#include <mutex>
#include <set>

namespace hydra
{
namespace vm
{

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

IRModule::IRModule(runtime::JSArray *stringsReferenced)
    : StringsReferenced(stringsReferenced)
{
    IRModuleGCHelper::GetInstance()->AddModule(this);
}

IRModule::~IRModule()
{
    IRModuleGCHelper::GetInstance()->RemoveModule(this);
}

} // namespace vm
} // namespace hydra