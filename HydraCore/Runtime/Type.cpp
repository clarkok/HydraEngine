#include "Type.h"

#include "JSObject.h"
#include "String.h"

namespace hydra
{
namespace runtime
{

JSObject *JSValue::Object() const
{
    auto cell = reinterpret_cast<gc::Cell*>(GetLast48Bit());
    auto object = dynamic_cast<JSObject*>(cell);

    hydra_assert(object || !cell,
        "dynamic_cast failed");

    return object;
}

String *JSValue::String() const
{
    auto cell = reinterpret_cast<gc::Cell*>(GetLast48Bit());
    auto string = dynamic_cast<runtime::String*>(cell);

    hydra_assert(string || !cell,
        "dynamic_cast failed");

    return string;
}

} // namespace runtime
} // namespace hydra