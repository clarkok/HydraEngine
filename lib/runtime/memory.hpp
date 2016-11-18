#pragma once

#include "common/common.hpp"
#include "common/assert.hpp"

#include <functional>

namespace hydra
{

using MemoryWriteWatcher = std::function<bool(void *)>;

void *AlignAlloc(size_t size, size_t aligment);
void AlignFree(void *ptr);
void WriteProtect(void *ptr, size_t size);
void WriteUnprotect(void *ptr, size_t size);
void SetMemoryWriteWatcher(MemoryWriteWatcher watcher);

}