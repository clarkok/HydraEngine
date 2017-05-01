#include "JSFunction.h"

namespace hydra
{
namespace runtime
{

bool JSNativeFunction::Call(JSValue thisArg, JSArray *arguments, JSValue &retVal, JSValue &error)
{
    try
    {
        retVal = Func(thisArg, arguments);
        return true;
    }
    catch (const JSValue &err)
    {
        error = err;
        return false;
    }
}

} // namespace runtime
} // namespace hydra