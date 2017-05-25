#ifndef _STRING_H_
#define _STRING_H_

#include "Common/HydraCore.h"
#include "Common/Platform.h"

#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/ThreadAllocator.h"

#include <vector>
#include <stack>
#include <string>
#include <algorithm>

namespace hydra
{
namespace runtime
{

struct OutOfRangeException : public Exception
{
public:
    OutOfRangeException(size_t required, size_t length)
        : Exception("OutOfRange: trying to access index " +
            std::to_string(required) + " of a string length " +
            std::to_string(length))
    { }
};

class String : public gc::HeapObject
{
public:
    virtual ~String() = default;

    virtual char_t at(size_t index) const = 0;
    virtual size_t length() const = 0;

    inline static String *New(gc::ThreadAllocator &allocator, const char16_t *str)
    {
        const char16_t *limit = str;
        while (*limit) limit++;

        return New(allocator, str, limit);
    }

    template <typename Iterator>
    static String *New(gc::ThreadAllocator &allocator, Iterator begin, Iterator end)
    {
        size_t length = std::distance(begin, end);
        size_t allocateSize = length * sizeof(char_t) + sizeof(ManagedString);

        return allocator.AllocateWithSizeAuto<ManagedString>(
            allocateSize, length, begin, end);
    }

    static String *Empty(gc::ThreadAllocator &allocator);
    static String *Concat(gc::ThreadAllocator &allocator, String *left, String *right);
    static String *Slice(gc::ThreadAllocator &allocator, String *sliced, size_t start, size_t length);

    inline bool EqualsTo(const String *other) const
    {
        if (length() != other->length())
        {
            return false;
        }

        if (GetHash() != other->GetHash())
        {
            return false;
        }

        for (size_t i = 0; i < length(); ++i)
        {
            if (_at(i) != other->_at(i))
            {
                return false;
            }
        }
        return true;
    }

    inline int Compare(const String *other) const
    {
#pragma push_macro("min")
#undef min
        size_t compareLength = std::min(length(), other->length());
#pragma pop_macro("min")
        for (size_t i = 0; i < compareLength; ++i)
        {
            if (_at(i) != other->_at(i))
            {
                return _at(i) - other->_at(i);
            }
        }
        if (length() != other->length())
        {
            return length() > other->length() ? 1 : -1;
        }
        return 0;
    }

    virtual void Scan(std::function<void(gc::HeapObject *)> scan) override final
    {
        if (Flattenned)
        {
            scan(Flattenned);
        }
        StringScan(scan);
    }

    String *Flatten(gc::ThreadAllocator &allocator);

    inline u64 GetHash() const
    {
        if (Hash != INVALID_HASH)
        {
            return Hash;
        }

        u64 m = 1;
        return Hash = hash(0, length(), m);
    }

    std::string ToString() const;

    static void Print(String *string);
    static void Println(String *string);

protected:
    static constexpr u64 INVALID_HASH = 0xFFFFFFFFFFFFFFFFull;
    static constexpr u64 HASH_MULTIPLIER = 6364136223846793005ull;

    String(u8 property)
        : HeapObject(property), Flattenned(nullptr), Hash(INVALID_HASH)
    { }

    virtual char_t _at(size_t) const = 0;
    virtual void StringScan(std::function<void(gc::HeapObject *)> scan) = 0;
    virtual void flatten(size_t start, size_t length, char_t *dst) const = 0;
    virtual u64 hash(size_t start, size_t end, u64 &m, u64 c = 0) const = 0;

    String *Flattenned;
    mutable u64 Hash;

private:
    friend class ConcatedString;
    friend class SlicedString;
};

class EmptyString : public String
{
public:
    EmptyString(u8 property)
        : String(property)
    { }

    virtual ~EmptyString() = default;

    virtual char_t at(size_t index) const override final
    {
        throw OutOfRangeException(index, 0);
    }

    virtual size_t length() const override final
    {
        return 0;
    }

protected:
    virtual char_t _at(size_t index) const override final
    {
        throw OutOfRangeException(index, 0);
    }

    virtual void StringScan(std::function<void(gc::HeapObject *)> scan) override final
    { }

    virtual void flatten(size_t start, size_t length, char_t *dst) const override final
    {
        if (start != 0 || length != 0)
        {
            throw OutOfRangeException(start, 0);
        }
    }

    virtual u64 hash(size_t start, size_t length, u64 &m, u64 c) const override final
    {
        if (start != 0 || length != 0)
        {
            throw OutOfRangeException(start, 0);
        }
        return c;
    }

private:
    friend class String;
};

class ManagedString : public String
{
public:
    ManagedString(u8 property, size_t length)
        : String(property), Length(length)
    { }

    template <typename Iterator>
    ManagedString(u8 property, size_t length, Iterator begin, Iterator end)
        : String(property), Length(length)
    {
        hydra_assert(std::distance(begin, end) <= static_cast<int>(length),
            "Distance should less or equal than length");
        std::copy(begin, end, this->begin());
    }

    virtual ~ManagedString() = default;

    virtual char_t at(size_t index) const override final
    {
        if (index >= Length)
        {
            throw OutOfRangeException(index, Length);
        }

        return _at(index);
    }

    virtual size_t length() const override final
    {
        return Length;
    }

protected:
    virtual char_t _at(size_t index) const override final
    {
        return begin()[index];
    }

    virtual void StringScan(std::function<void(gc::HeapObject *)> scan) override final
    { }

    virtual void flatten(size_t start, size_t length, char_t *dst) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        std::copy(begin() + start, begin() + start + length, dst);
    }

    virtual u64 hash(size_t start, size_t length, u64 &m, u64 c) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        if (start == 0 && length == this->length() && Hash != INVALID_HASH)
        {
            c += Hash * m;
            m *= platform::powi(HASH_MULTIPLIER, length);
            return c;
        }

        for (char_t *ptr = begin() + start, *limit = begin() + start + length;
            ptr != limit;
            ptr++)
        {
            c += (*ptr) * m;
            m *= HASH_MULTIPLIER;
        }
        return c;
    }

private:
    char_t *begin() const
    {
        return reinterpret_cast<char_t *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(ManagedString));
    }

    char_t *end() const
    {
        return begin() + length();
    }

    size_t Length;

    friend class String;
};

class ConcatedString : public String
{
public:
    ConcatedString(u8 property, String *left, String *right)
        : String(property), Left(left), Right(right), LeftLength(left->length()),
        Length(left->length() + right->length())
    {
        gc::Heap::GetInstance()->WriteBarrier(this, left);
        gc::Heap::GetInstance()->WriteBarrier(this, right);
    }

    virtual ~ConcatedString() = default;

    virtual char_t at(size_t index) const override final
    {
        if (index >= Length)
        {
            throw OutOfRangeException(index, Length);
        }

        return _at(index);
    }

    virtual size_t length() const override final
    {
        return Length;
    }

protected:
    virtual char_t _at(size_t index) const override final
    {
        if (index < LeftLength)
        {
            return Left->_at(index);
        }
        else
        {
            return Right->_at(index - LeftLength);
        }
    }

    virtual void StringScan(std::function<void(gc::HeapObject *)> scan) override final
    {
        scan(Left);
        scan(Right);
    }

    virtual void flatten(size_t start, size_t length, char_t *dst) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        if (start < LeftLength)
        {
            size_t copyLength = std::min<size_t>(length, LeftLength - start);
            Left->flatten(start, copyLength, dst);
            dst += copyLength;
        }

        if (start + length >= LeftLength)
        {
            size_t copyStart = start > LeftLength ? start - LeftLength : 0;
            Right->flatten(copyStart, start + length - LeftLength, dst);
        }
    }

    virtual u64 hash(size_t start, size_t length, u64 &m, u64 c) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        if (start == 0 && length == this->length() && Hash != INVALID_HASH)
        {
            c += Hash * m;
            m *= platform::powi(HASH_MULTIPLIER, length);
            return c;
        }

        if (start < LeftLength)
        {
            size_t hashLength = std::min<size_t>(length, LeftLength - start);
            c = Left->hash(start, hashLength, m, c);
        }

        if (start + length >= LeftLength)
        {
            size_t hashStart = start > LeftLength ? start - LeftLength : 0;
            c = Right->hash(hashStart, start + length - LeftLength, m, c);
        }

        return c;
    }

private:
    String *Left;
    String *Right;
    size_t LeftLength;
    size_t Length;

    friend class String;
};

class SlicedString : public String
{
public:
    SlicedString(u8 property, String *sliced, size_t start, size_t length)
        : String(property), Sliced(sliced), Start(start), Length(length)
    {
        hydra_assert(length + start <= sliced->length(),
            "Slice must be contained by the sliced");
        gc::Heap::GetInstance()->WriteBarrier(this, sliced);
    }

    virtual ~SlicedString() = default;

    virtual char_t at(size_t index) const override final
    {
        if (index >= Length)
        {
            throw OutOfRangeException(index, Length);
        }

        return _at(index);
    }

    virtual size_t length() const override final
    {
        return Length;
    }

protected:
    virtual char_t _at(size_t index) const override final
    {
        return Sliced->_at(index + Start);
    }

    virtual void StringScan(std::function<void(gc::HeapObject *)> scan) override final
    {
        scan(Sliced);
    }

    virtual void flatten(size_t start, size_t length, char_t *dst) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        Sliced->flatten(start + Start, length, dst);
    }

    virtual u64 hash(size_t start, size_t length, u64 &m, u64 c) const override final
    {
        if (start + length > Length)
        {
            throw OutOfRangeException(start + length, Length);
        }

        if (start == 0 && length == this->length() && Hash != INVALID_HASH)
        {
            c += Hash * m;
            m *= platform::powi(HASH_MULTIPLIER, length);
            return c;
        }

        return Sliced->hash(start + Start, length, m, c);
    }

private:
    String *Sliced;
    size_t Start;
    size_t Length;

    friend class String;
};

} // namespace runtime
} // namespace hydra

#endif // _STRING_H_