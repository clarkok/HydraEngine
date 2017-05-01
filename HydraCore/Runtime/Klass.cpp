#include "Klass.h"

namespace hydra
{
namespace runtime
{

Klass *Klass::EmptyKlass(gc::ThreadAllocator &allocator)
{
    static std::atomic<Klass*> emptyKlass{ nullptr };

    auto currentKlass = emptyKlass.load();
    if (!currentKlass)
    {
        auto newKlass = allocator.AllocateAuto<Klass>(
            gc::Region::GetLevelFromSize(sizeof(Klass)),
            nullptr);

        if (emptyKlass.compare_exchange_strong(currentKlass, newKlass))
        {
            currentKlass = newKlass;
        }
    }

    return currentKlass;
}

} // runtime
} // hydra