#ifndef _HYDRA_CORE_H_
#define _HYDRA_CORE_H_

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace hydra
{

using namespace std::chrono_literals;

using std::size_t;
using std::uintptr_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using char_t = char16_t;

using std::string;

struct Exception : std::exception
{
    std::string What;

    Exception(std::string what)
        : std::exception(), What(what)
    { }

    virtual const char *what() const noexcept override
    {
        return What.c_str();
    }
};

struct TrappedException : Exception
{
    TrappedException(
        std::string source,
        int lineNumber,
        std::string message
    )
        : Exception(
            "Trapped: " + source + " line " + std::to_string(lineNumber)
            + "\n  " + message)
    { }

    static inline void Trap(std::string source, int lineNumber, std::string message)
    {
        throw TrappedException(source, lineNumber, message);
    }
};

#define trap(msg)   ::hydra::TrappedException::Trap(__FILE__, __LINE__, msg)

struct AssertFailureException : Exception
{
    AssertFailureException(
        std::string source,
        int lineNumber,
        std::string expression,
        std::string message
    )
        : Exception(
            "AssertFailure: (" + expression + ") != true at " + source + " line " + std::to_string(lineNumber)
            + "\n  " + message)
    { }

    static inline void TestAndRaise(
        std::string source,
        int lineNumber,
        bool condition,
        std::string expression,
        std::string message)
    {
        if (!condition)
        {
            throw AssertFailureException(
                source,
                lineNumber,
                expression,
                message);
        }
    }
};

#define hydra_assert(expr, msg) ::hydra::AssertFailureException::TestAndRaise(__FILE__, __LINE__, (expr), #expr, msg)

}

#endif // _HYDRA_CORE_H_