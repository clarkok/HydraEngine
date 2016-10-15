#pragma once

#include "common.hpp"

#include <exception>
#include <string>

namespace hydra
{

struct Exception : public std::exception
{
    std::string Message;

    Exception(std::string message)
        : Message(message)
    { }

    virtual const char * what() const
    {
        return Message.c_str();
    }
};

}