#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "HydraCore.h"

#ifdef _MSC_VER

#include <intrin.h>
#include <windows.h>

#else
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

    for (uintptr_t ptr = threadContext.Rsp; ptr < high; ptr += sizeof(void*))
    {
        callback(reinterpret_cast<void**>(ptr));
    }
}

#else
#endif


}
}

#endif // _PLATFORM_H_