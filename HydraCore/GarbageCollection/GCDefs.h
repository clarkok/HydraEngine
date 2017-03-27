#ifndef _GC_DEFS_H_
#define _GC_DEFS_H_

#include "Common/HydraCore.h"

namespace hydra
{

namespace gc
{

constexpr size_t REGION_SIZE_LEVEL = 19;
constexpr size_t REGION_SIZE = 1u << REGION_SIZE_LEVEL;     // 512 KB

constexpr size_t MAXIMAL_ALLOCATE_SIZE_LEVEL = REGION_SIZE_LEVEL - 2;
constexpr size_t MAXIMAL_ALLOCATE_SIZE = 1u << MAXIMAL_ALLOCATE_SIZE_LEVEL;

constexpr size_t MINIMAL_ALLOCATE_SIZE_LEVEL = 5;
constexpr size_t MINIMAL_ALLOCATE_SIZE = 1u << MINIMAL_ALLOCATE_SIZE_LEVEL;

constexpr size_t LEVEL_NR = MAXIMAL_ALLOCATE_SIZE_LEVEL - MINIMAL_ALLOCATE_SIZE_LEVEL + 1;

constexpr auto GC_CHECK_INTERVAL = 1000ms;

constexpr size_t GC_WORKER_MAX_NR = 8;

constexpr size_t GC_WORKER_BALANCE_FACTOR = 128;
constexpr double GC_WORKING_QUEUE_FACTOR = 0.7;

constexpr size_t GC_FULL_GC_TRIGGER_FACTOR = 256;

} // namespace gc

} // namespace hydra

#endif // _GC_DEFS_H_