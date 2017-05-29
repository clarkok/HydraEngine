#include "HydraCore.h"

#include <iostream>

namespace hydra
{

Exception::Exception(std::string what)
    : std::exception(), What(what)
{
    std::cerr << "EXCEPTION: " << what << std::endl;
}

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