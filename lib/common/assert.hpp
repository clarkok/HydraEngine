#pragma once

#include "exception.hpp"

namespace hydra
{

struct AssertFailedException : Exception
{
    AssertFailedException(std::string source, int line, std::string cond, std::string message)
        : Exception("Assert Failed: " + cond + " : " + message + "\n    " + source + " line " + std::to_string(line))
    { }
};

#define _AssertImpl(src, line, cond, message)                       \
    do                                                              \
    {                                                               \
        if (!(cond))                                                \
        {                                                           \
            throw AssertFailedException(src, line, #cond, message); \
        }                                                           \
    } while (0)

#define Assert(cond, message)   _AssertImpl(__FILE__, __LINE__, cond, message)

}