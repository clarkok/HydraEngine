#include "memory.hpp"
#include "common/utility.hpp"

#include <malloc.h>
#include <list>
#include <mutex>

#ifdef _WIN64
#include <Windows.h>
#endif

namespace hydra
{

namespace memory
{

#ifdef _WIN64

void *AlignAlloc(size_t size, size_t alignment)
{
    return _aligned_malloc(size, alignment);
}

void AlignFree(void *ptr)
{
    _aligned_free(ptr);
}

void WriteProtect(void *ptr, size_t size)
{
    DWORD original;
    PDWORD pOriginal = &original;
    auto result = VirtualProtect(ptr, size, PAGE_READONLY, pOriginal);
    Assert(result != 0, "Write protected failed, error code: ", GetLastError());
}

void WriteProtect(volatile void *ptr, size_t size)
{
    WriteProtect((void*)ptr, size);
}

void WriteUnprotect(void *ptr, size_t size)
{
    DWORD original;
    PDWORD pOriginal = &original;
    auto result = VirtualProtect(ptr, size, PAGE_READWRITE, pOriginal);
    Assert(result != 0, "Write unprotected failed, error code: ", GetLastError());
}

void WriteUnprotect(volatile void *ptr, size_t size)
{
    WriteUnprotect((void*)ptr, size);
}

static std::list<MemoryWriteWatcher> globalWatchers;
static std::mutex globalWatchersMutex;

static LONG ExceptionHandler(LPEXCEPTION_POINTERS except)
{
    std::lock_guard<std::mutex> guard(globalWatchersMutex);

    if (except->ExceptionRecord->ExceptionCode != STATUS_ACCESS_VIOLATION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    UINT_PTR addr = except->ExceptionRecord->ExceptionInformation[1];

    for (const auto &watcher : globalWatchers)
    {
        if (watcher((volatile void*)addr))
        {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

BEFORE_MAIN({
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ExceptionHandler);
});

uint64_t AddMemoryWriteWatcher(MemoryWriteWatcher watcher)
{
    std::lock_guard<std::mutex> guard(globalWatchersMutex);
    globalWatchers.push_back(watcher);
    return reinterpret_cast<uint64_t>(&globalWatchers.back());
}

void RemoveMemoryWriteWatcher(uint64_t handle)
{
    std::lock_guard<std::mutex> guard(globalWatchersMutex);
    globalWatchers.remove_if([&](const MemoryWriteWatcher &w)
    {
        return reinterpret_cast<uint64_t>(&w) == handle;
    });
}

#else
static_assert(false, "Not implemented");
#endif

}

}
