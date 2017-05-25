#include "String.h"

#include <iostream>

namespace hydra
{
namespace runtime
{

String *String::Empty(gc::ThreadAllocator &allocator)
{
    return allocator.AllocateAuto<EmptyString>();
}

String *String::Concat(
    gc::ThreadAllocator &allocator,
    String *left,
    String *right)
{
    return allocator.AllocateAuto<ConcatedString>(left, right);
}

String *String::Slice(
    gc::ThreadAllocator &allocator,
    String *sliced,
    size_t start,
    size_t length)
{
    return allocator.AllocateAuto<SlicedString>(sliced, start, length);
}

String *String::Flatten(gc::ThreadAllocator &allocator)
{
    if (Flattenned)
    {
        return Flattenned;
    }

    size_t allocateSize = length() + sizeof(ManagedString);
    auto flattenned = allocator.AllocateWithSizeAuto<ManagedString>(allocateSize, length());

    flatten(0, length(), flattenned->begin());
    Flattenned = flattenned;

    gc::Heap::GetInstance()->WriteBarrier(this, Flattenned);
    return Flattenned;
}

std::string String::ToString() const
{
    std::string out;
    out.reserve(length());  // this might not be enough

    for (size_t i = 0; i < length(); ++i)
    {
        u32 code;
        u16 ch = at(i);
        if ((ch & 0xFC00) == 0xD800)
        {
            code = ((ch & 0x3FF) << 10) + 0x10000;
            code |= (at(++i) & 0x3FF);
        }
        else
        {
            code = ch;
        }

        if (code < 0x80)
        {
            out.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else if (code < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            // TODO codeeck code range
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    return out;
}

void String::Print(String *string)
{
    std::cout << string->ToString();
}

void String::Println(String *string)
{
    Print(string);
    std::cout << std::endl;
}

} // namespace runtime
} // namespace hydra