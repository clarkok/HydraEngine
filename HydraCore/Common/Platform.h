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
    u64 low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    uintptr_t addressOfReturnAddress = reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());

    for (uintptr_t ptr = low < addressOfReturnAddress ? addressOfReturnAddress : low; ptr < high; ptr += sizeof(void*))
    {
        callback(reinterpret_cast<void**>(ptr));
    }
}

#else
#endif


}
}

#endif // _PLATFORM_H_