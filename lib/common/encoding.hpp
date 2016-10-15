#pragma once

#include "common.hpp"

#include <type_traits>

namespace hydra
{ namespace Encoding
{

namespace internal
{
    template<typename T, typename U>
    struct IsInputIteratorOf : std::is_convertible<decltype(*(*(T*)0)), const U &>
    { };

    template <typename T, typename U>
    struct IsOutputIteratorOf : std::is_convertible<decltype(*(*(T*)0)), U &>
    { };
}

template<typename T, typename = internal::IsOutputIteratorOf<T, uint8_t> >
T EncodeUtf8(T dst, uint32_t ch)
{
    if (ch < 0x80)
    {
        *dst++ = ch;
    }
    else if (ch < 0x800)
    {
        *dst++ = 0xC0 | (ch >> 6);
        *dst++ = 0x80 | (ch & 0x3F);
    }
    else if (ch < 0x10000)
    {
        *dst++ = 0xE0 | (ch >> 12);
        *dst++ = 0x80 | ((ch >> 6) & 0x3F);
        *dst++ = 0x80 | (ch & 0x3F);
    }
    else
    {
        // TODO check code range
        *dst++ = 0xF0 | (ch >> 18);
        *dst++ = 0x80 | ((ch >> 12) & 0x3F);
        *dst++ = 0x80 | ((ch >> 6) & 0x3F);
        *dst++ = 0x80 | (ch & 0x3F);
    }
    return dst;
}

template<typename T, typename = internal::IsInputIteratorOf<T, uint8_t> >
T DecodeUtf8(T src, uint32_t &decoded)
{
    if (((*src) & 0x80) == 0)
    {
        decoded = *src++;
    }
    else if (((*src) & 0xE0) == 0xC0)
    {
        decoded = ((*src++) & 0x1F) << 6;
        decoded |= ((*src++) & 0x3F);
    }
    else if (((*src) & 0xF0) == 0xE0)
    {
        decoded = ((*src++) & 0x0F) << 12;
        decoded |= ((*src++) & 0x3F) << 6;
        decoded |= ((*src++) & 0x3F);
    }
    else if (((*src) & 0xF8) == 0xF0)
    {
        decoded = ((*src++) & 0x07) << 18;
        decoded |= ((*src++) & 0x3F) << 12;
        decoded |= ((*src++) & 0x3F) << 6;
        decoded |= ((*src++) & 0x3F);
    }
    return src;
}

template<typename T, typename = internal::IsOutputIteratorOf<T, uint16_t> >
T EncodeUtf16(T dst, uint32_t ch)
{
    if ((ch < 0xD800) || (ch >= 0xE000 && ch < 0x10000))
    {
        *dst++ = ch;
    }
    else
    {
        *dst++ = ((ch - 0x10000) >> 10) | 0xD800;
        *dst++ = (ch & 0x3FF) | 0xDC00;
    }
    return dst;
}

template<typename T, typename = internal::IsInputIteratorOf<T, uint16_t> >
T DecodeUtf16(T src, uint32_t &decoded)
{
    if ((*src & 0xFC00) == 0xD800)
    {
        decoded = (((*src++) & 0x3FF) << 10) + 0x10000;
        decoded |= ((*src++) & 0x3FF);
    }
    else
    {
        decoded = *src++;
    }
    return src;
}

template<typename T, typename U,
    typename = internal::IsOutputIteratorOf<T, uint8_t>,
    typename = internal::IsInputIteratorOf<U, uint16_t>
>
T ConvertUtf16To8(T dst, U src, U limit)
{
    while (src < limit)
    {
        uint32_t decoded;
        src = DecodeUtf16(src, decoded);
        dst = EncodeUtf8(dst, decoded);
    }
    return dst;
}

template<typename T, typename U,
    typename = internal::IsOutputIteratorOf<T, uint16_t>
    typename = internal::IsInputIteratorOf<U, uint8_t>
>
T ConvertUtf8To16(T dst, U src, U limit)
{
    while (src != limit)
    {
        dst += EncodeUtf8(DecodeUtf16(src++), dst);
    }
    return dst;
}

}}