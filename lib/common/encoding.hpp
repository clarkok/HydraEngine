#pragma once

#include "common.hpp"

#include <type_traits>

namespace hydra
{ namespace Encoding
{

template<typename T, typename = std::is_convertible<decltype(*(*(T*)0)), uint8_t &> >
size_t EncodeUtf8(uint32_t ch, T dst)
{
    if (ch < 0x80)
    {
        *dst = ch;
        return 1;
    }
    else if (ch < 0x800)
    {
        *dst++ = 0xC0 | (ch >> 6);
        *dst = 0x80 | (ch & 0x3F);
        return 2;
    }
    else if (ch < 0x10000)
    {
        *dst++ = 0xE0 | (ch >> 12);
        *dst++ = 0x80 | ((ch >> 6) & 0x3F);
        *dst = 0x80 | (ch & 0x3F);
        return 3;
    }
    else
    {
        // TODO check code range
        *dst++ = 0xF0 | (ch >> 18);
        *dst++ = 0x80 | ((ch >> 12) & 0x3F);
        *dst++ = 0x80 | ((ch >> 6) & 0x3F);
        *dst = 0x80 | (ch & 0x3F);
        return 4;
    }
}

template<typename T, typename = std::is_convertible<decltype(*(*(T*)0)), const uint8_t &> >
uint32_t DecodeUtf8(T src)
{
    if (((*src) & 0x80) == 0)
    {
        return *src;
    }
    else if (((*src) & 0xE0) == 0xC0)
    {
        uint32_t decoded = ((*src++) & 0x1F) << 6;
        return decoded | ((*src) & 0x3F);
    }
    else if (((*src) & 0xF0) == 0xE0)
    {
        uint32_t decoded = ((*src++) & 0x0F) << 12;
        decoded |= ((*src++) & 0x3F) << 6;
        return decoded | ((*src) & 0x3F);
    }
    else if (((*src) & 0xF8) == 0xF0)
    {
        uint32_t decoded = ((*src++) & 0x07) << 18;
        decoded |= ((*src++) & 0x3F) << 12;
        decoded |= ((*src++) & 0x3F) << 6;
        return decoded | ((*src) & 0x3F);
    }
    else
    {
        *(int*)(0) = 1;
        return 0;
    }
}

template<typename T, typename = std::is_convertible<decltype(*(*(T*)0)), uint16_t &>>
size_t EncodeUtf16(uint32_t ch, T dst)
{
    if ((ch < 0xD800) || (ch >= 0xE000 && ch < 0x10000))
    {
        *dst = ch;
        return 1;
    }
    else
    {
        *dst++ = ((ch - 0x10000) >> 10) | 0xD800;
        *dst = (ch & 0x3FF) | 0xDC00;
        return 2;
    }
}

template<typename T, typename = std::is_convertible<decltype(*(*(T*)0)), const uint16_t &>>
uint32_t DecodeUtf16(T src)
{
    if ((*src & 0xFC00) == 0xD800)
    {
        uint32_t decoded = (((*src++) & 0x3FF) << 10) + 0x10000;
        return decoded | ((*src) & 0x3FF);
    }
    else
    {
        return *src;
    }
}

}}