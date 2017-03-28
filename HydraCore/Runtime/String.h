#ifndef _STRING_H_
#define _STRING_H_

#include "Common/HydraCore.h"
#include "GarbageCollection/HeapObject.h"
#include "GarbageCollection/ThreadAllocator.h"

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

    virtual char_t at(size_t) const = 0;
    virtual size_t length() const = 0;

    template <typename Iterator, typename T_Report>
    static String *New(gc::ThreadAllocator &allocator, T_Report reportFunc, Iterator begin, Iterator end)
    {
        size_t length = std::distance(begin, end);
        size_t allocateSize = length + sizeof(ManagedString);

        return allocator.AllocateWithSize<ManagedString>(
            allocateSize, reportFunc, length, begin, end);
    }

    template <typename T_Report>
    static String *Empty(gc::ThreadAllocator &allocator, T_Report reportFunc)
    {
        return allocator.Allocate<EmptyString>(reportFunc);
    }

protected:
    String(u8 property)
        : HeapObject(property)
    { }

    virtual char_t _at(size_t) const = 0;

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

    virtual void Scan(std::function<void(gc::HeapObject *)> scan) override final
    { }

protected:
    virtual char_t _at(size_t index) const override final
    {
        throw OutOfRangeException(index, 0);
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

    virtual void Scan(std::function<void(gc::HeapObject *)> scan) override final
    { }

protected:
    virtual char_t _at(size_t index) const override final
    {
        return begin()[index];
    }

private:
    char_t *begin() const
    {
        return reinterpret_cast<char_t *>(
            reinterpret_cast<uintptr_t>(this) + sizeof(ManagedString));
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
    { }

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

    virtual void Scan(std::function<void(gc::HeapObject *)> scan) override final
    {
        scan(Left);
        scan(Right);
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
    }

    virtual ~SlicedString() = default;

    virtual char_t at(size_t index) const override final
    {
        if (index >= Length)
        {
            throw OutOfRangeException(index, Length);
        }
    }

    virtual size_t length() const override final
    {
        return Length;
    }


    virtual void Scan(std::function<void(gc::HeapObject *)> scan) override final
    {
        scan(Sliced);
    }

protected:
    virtual char_t _at(size_t index) const override final
    {
        return Sliced->_at(index - Start);
    }

public:
    String *Sliced;
    size_t Start;
    size_t Length;

    friend class String;
};

} // namespace runtime
} // namespace hydra

#endif // _STRING_H_