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

#define HYDRA_ENABLE_LOG
#define HYDRA_LOG_TO_FILE

using namespace std::chrono_literals;

using std::size_t;
using std::uintptr_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

static_assert(sizeof(u8) == 1, "sizeof(u8) == 1");
static_assert(sizeof(u16) == 2, "sizeof(u16) == 2");
static_assert(sizeof(u32) == 4, "sizeof(u32) == 4");
static_assert(sizeof(u64) == 8, "sizeof(u64) == 8");

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

static_assert(sizeof(i8) == 1, "sizeof(i8) == 1");
static_assert(sizeof(i16) == 2, "sizeof(i16) == 2");
static_assert(sizeof(i32) == 4, "sizeof(i32) == 4");
static_assert(sizeof(i64) == 8, "sizeof(i64) == 8");

using char_t = char16_t;

using std::string;

struct Exception : std::exception
{
    std::string What;

    Exception(std::string what);

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

#define hydra_trap(msg)   ::hydra::TrappedException::Trap(__FILE__, __LINE__, msg)

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

    static void Raise(
        const char *source,
        int lineNumber,
        const char *expression,
        const char *message);
};

#define hydra_assert(expr, msg)                                                     \
    do {                                                                            \
        if (!(expr))                                                                \
            ::hydra::AssertFailureException::Raise(__FILE__, __LINE__, #expr, msg); \
    } while (false)


#define __hydra_rep__0(_stat)
#define __hydra_rep__1(_stat)   _stat
#define __hydra_rep__2(_stat)   _stat __hydra_rep__1(_stat)
#define __hydra_rep__3(_stat)   _stat __hydra_rep__2(_stat)
#define __hydra_rep__4(_stat)   _stat __hydra_rep__3(_stat)
#define __hydra_rep__5(_stat)   _stat __hydra_rep__4(_stat)
#define __hydra_rep__6(_stat)   _stat __hydra_rep__5(_stat)
#define __hydra_rep__7(_stat)   _stat __hydra_rep__6(_stat)
#define __hydra_rep__8(_stat)   _stat __hydra_rep__7(_stat)
#define __hydra_rep__9(_stat)   _stat __hydra_rep__8(_stat)

#define hydra_rep(_n, _stat)    __hydra_rep__##_n(_stat)

}

#endif // _HYDRA_CORE_H_