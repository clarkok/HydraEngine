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

#else
#endif


}
}

#endif // _PLATFORM_H_