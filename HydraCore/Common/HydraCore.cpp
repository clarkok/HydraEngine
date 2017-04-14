#include "HydraCore.h"

namespace hydra
{

void AssertFailureException::Raise(
    const char *source,
    int lineNumber,
    const char *expression,
    const char *message
)
{
    throw AssertFailureException(
        source,
        lineNumber,
        expression,
        message);
}

}