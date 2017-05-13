#ifndef _BYTECODE_H_
#define _BYTECODE_H_

#include "Common/HydraCore.h"
#include "Common/Platform.h"

#include "GarbageCollection/GC.h"

#include "IR.h"

#include <vector>
#include <map>
#include <memory>

namespace hydra
{
namespace vm
{

enum ByteCodeSectionType
{
    FUNCTION = 0,
    STRING_POOL = 1
};

class SectionReader;

class ByteCode
{
public:
    ByteCode(std::string filename);

    template <typename T>
    T *View(size_t offset)
    {
        void *view = FileMapping;
        return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(view) + offset);
    }

    std::unique_ptr<IRModule> Load(gc::ThreadAllocator &allocator);

    static constexpr u32 MAGIC_WORD = 0x52495948;   // 'H', 'Y', 'I', 'R'

private:
    bool ParseHeader();

    void LoadStringPool(gc::ThreadAllocator &allocator,
        runtime::JSArray *strings,
        std::map<u32, runtime::String *> &stringMap);

    std::unique_ptr<IRFunc> LoadFunction(
        size_t section,
        std::map<u32, runtime::String *> &stringMap);

    platform::MappedFile FileMapping;
    std::vector<std::pair<u32, u32>> Sections;
    u32 StringPoolOffset;
};

class SectionReader
{
public:
    SectionReader(ByteCode *owner, size_t offset)
        : Owner(owner), Start(offset + 4), Offset(offset + 4), Limit(*(owner->View<u32>(offset)) + offset + 4)
    { }

    inline bool Uint(u32 &out)
    {
        if (!Check(4))
        {
            return false;
        }

        hydra_assert((Offset & 3) == 0,
            "Offset should be aligned");

        out = *(Owner->View<u32>(Offset));
        Offset += 4;
        return true;
    }

    inline bool Double(double &out)
    {
        if (!Check(8))
        {
            return false;
        }

        out = *(Owner->View<double>(Offset));
        Offset += 8;
        return true;
    }

    inline bool String(u32 &length, u32 &start, std::pair<const char_t *, const char_t *> &range)
    {
        if (!Align(4))
        {
            return false;
        }

        start = Current();

        if (!Uint(length))
        {
            return false;
        }

        if (!Check(length * sizeof(char_t)))
        {
            return false;
        }

        range.first = Owner->View<char_t>(Offset);
        range.second = range.first + length;
        Offset += length * sizeof(char_t);

        return true;
    }

    inline size_t Current() const
    {
        return Offset - Start;
    }

private:
    ByteCode *Owner;
    size_t Start;
    size_t Offset;
    size_t Limit;

    inline bool Check(size_t size)
    {
        return (Offset + size) <= Limit;
    }

    inline bool Align(size_t size)
    {
        auto mod = Offset % size;
        if (mod)
        {
            Offset += (size - mod);
            return Check(0);
        }
        return true;
    }
};

}
}

#endif // _BYTECODE_H_