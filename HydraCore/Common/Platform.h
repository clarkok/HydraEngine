#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "HydraCore.h"

#ifdef _MSC_VER

#include <intrin.h>
#include <windows.h>

#else

#include <stdlib.h>

#endif

namespace hydra
{
namespace platform
{

inline void *AlignedAlloc(size_t size, size_t alignment);
inline void AlignedFree(void *ptr);
inline size_t GetMSB(uint64_t);
inline size_t GetLSB(uint64_t);
inline u64 powi(u64 base, u64 exp)
{
    u64 res = 1;
    while (exp)
    {
        if (exp & 1)
        {
            res *= base;
        }
        exp >>= 1;
        base *= base;
    }
    return res;
}

template <typename T_callback>
void ForeachWordOnStack(T_callback callback);

#ifdef _MSC_VER

inline void *AlignedAlloc(size_t size, size_t alignment)
{
    return _aligned_malloc(size, alignment);
}

inline void AlignedFree(void *ptr)
{
    _aligned_free(ptr);
}

inline size_t GetMSB(uint64_t value)
{
    unsigned long ret;
    if (_BitScanReverse64(&ret, value))
    {
        return ret;
    }

    return static_cast<size_t>(-1);
}

inline size_t GetLSB(uint64_t value)
{
    unsigned long ret;
    if (_BitScanForward64(&ret, value))
    {
        return ret;
    }

    return static_cast<size_t>(-1);
}

template <typename T_callback>
void ForeachWordOnStack(T_callback callback)
{
    CONTEXT threadContext;
    RtlCaptureContext(&threadContext);

    u64 low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);

    hydra_assert(reinterpret_cast<uintptr_t>(&threadContext) >= threadContext.Rsp &&
        reinterpret_cast<uintptr_t>(&threadContext) < high,
        "threadContext must in stack");

    for (uintptr_t ptr = threadContext.Rsp; ptr < high; ptr += sizeof(void*))
    {
        callback(reinterpret_cast<void**>(ptr));
    }
}

#else

inline void *AlignedAlloc(size_t size, size_t alignment)
{
    void *ret = nullptr;
    if (posix_memalign(&ret, alignment, size))
    {
        return nullptr;
    }
    return ret;
}

inline void AlignedFree(void *ptr)
{
    free(ptr);
}

inline size_t GetMSB(uint64_t value)
{
    if (value) {
        return 63 - __builtin_clz(value);
    }
    return static_cast<size_t>(-1);
}

inline size_t GetLSB(uint64_t value)
{
    if (value) {
        return  __builtin_ctz(value);
    }
    return static_cast<size_t>(-1);
}

template <typename T_callback>
void ForeachWordOnStack(T_callback callback)
{
    uintptr_t stackTop = 0;
    uintptr_t stackEnd = 0;

    {
        asm("movq %%rbp, %0"
            : "=r" (stackTop));

        void **ptr = reinterpret_cast<void**>(stackTop);
        while (*ptr) {
            ptr = reinterpret_cast<void**>(*ptr);
        }
        stackEnd = reinterpret_cast<uintptr_t>(ptr);
    }

    for (uintptr_t ptr = stackTop; ptr < stackEnd; ptr += sizeof(void*))
    {
        callback(reinterpret_cast<void**>(ptr));
    }
}

#endif


}
}

#endif // _PLATFORM_H_
