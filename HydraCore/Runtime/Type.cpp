#include "Type.h"

#include "JSObject.h"
#include "String.h"

namespace hydra
{
namespace runtime
{

JSObject *JSValue::Object() const
{
    return dynamic_cast<JSObject*>(
        reinterpret_cast<gc::HeapObject*>(GetLast48Bit()));
}

String *JSValue::String() const
{
    return dynamic_cast<runtime::String*>(
        reinterpret_cast<gc::HeapObject*>(GetLast48Bit()));
}

} // namespace runtime
} // namespace hydra