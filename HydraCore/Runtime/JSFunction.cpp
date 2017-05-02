#include "JSFunction.h"

namespace hydra
{
namespace runtime
{

bool JSNativeFunction::Call(gc::ThreadAllocator &allocator,
    JSValue thisArg,
    JSArray *arguments,
    JSValue &retVal,
    JSValue &error)
{
    return Func(allocator, thisArg, arguments, retVal, error);
}

} // namespace runtime
} // namespace hydra