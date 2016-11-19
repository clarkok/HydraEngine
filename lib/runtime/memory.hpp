#pragma once

#include "common/common.hpp"
#include "common/assert.hpp"

#include <functional>

namespace hydra
{

namespace memory
{

using MemoryWriteWatcher = std::function<bool(volatile void *)>;

void *AlignAlloc(size_t size, size_t aligment);
void AlignFree(void *ptr);
void WriteProtect(void *ptr, size_t size);
void WriteUnprotect(void *ptr, size_t size);
void WriteProtect(volatile void *ptr, size_t size);
void WriteUnprotect(volatile void *ptr, size_t size);
uint64_t AddMemoryWriteWatcher(MemoryWriteWatcher watcher);
void RemoveMemoryWriteWatcher(uint64_t handle);

}

}