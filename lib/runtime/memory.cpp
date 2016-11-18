#include "memory.hpp"
#include "common/utility.hpp"

#include <malloc.h>

#ifdef _WIN64
#include <Windows.h>
#endif

namespace hydra
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

void WriteUnprotect(void *ptr, size_t size)
{
    DWORD original;
    PDWORD pOriginal = &original;
    auto result = VirtualProtect(ptr, size, PAGE_READWRITE, pOriginal);
    Assert(result != 0, "Write unprotected failed, error code: ", GetLastError());
}

MemoryWriteWatcher globalWatcher;

static LONG ExceptionHandler(LPEXCEPTION_POINTERS except)
{
    if (except->ExceptionRecord->ExceptionCode != STATUS_ACCESS_VIOLATION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    UINT_PTR addr = except->ExceptionRecord->ExceptionInformation[1];
    if (globalWatcher && globalWatcher((void*)addr))
    {
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

BEFORE_MAIN({
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ExceptionHandler);
});

void SetMemoryWriteWatcher(MemoryWriteWatcher watcher)
{
    globalWatcher = watcher;
}

#else
static_assert(false, "Not implemented");
#endif

}
