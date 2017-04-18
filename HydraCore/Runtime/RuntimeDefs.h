#ifndef _RUNTIME_DEFS_H_
#define _RUNTIME_DEFS_H_

#include "Common/HydraCore.h"

namespace hydra
{
namespace runtime
{

constexpr static size_t MINIMAL_KEY_COUNT_TO_ENABLE_HASH_IN_KLASS = 8;
constexpr static size_t DEFAULT_TRANSACTION_SIZE_IN_KLASS = 16;

} // namespace runtime
} // namespace hydra

#endif // _RUNTIME_DEFS_H_