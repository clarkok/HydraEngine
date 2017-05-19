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

void String::Print(String *string)
{
    for (size_t i = 0; i < string->length(); ++i)
    {
        u32 code;
        u16 ch = string->at(i);
        if ((ch & 0xFC00) == 0xD800)
        {
            code = ((ch & 0x3FF) << 10) + 0x10000;
            code |= (string->at(++i) & 0x3FF);
        }
        else
        {
            code = ch;
        }

        if (code < 0x80)
        {
            std::cout << static_cast<char>(code);
        }
        else if (code < 0x800)
        {
            std::cout << static_cast<char>(0xC0 | (code >> 6));
            std::cout << static_cast<char>(0x80 | (code & 0x3F));
        }
        else if (code < 0x10000)
        {
            std::cout << static_cast<char>(0xE0 | (code >> 12));
            std::cout << static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            std::cout << static_cast<char>(0x80 | (code & 0x3F));
        }
        else
        {
            // TODO codeeck code range
            std::cout << static_cast<char>(0xF0 | (code >> 18));
            std::cout << static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            std::cout << static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            std::cout << static_cast<char>(0x80 | (code & 0x3F));
        }
    }
}

void String::Println(String *string)
{
    Print(string);
    std::cout << std::endl;
}

} // namespace runtime
} // namespace hydra