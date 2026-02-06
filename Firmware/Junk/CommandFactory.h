// CommandFactory.h
#pragma once
#include "CommandInterface.h"

// One shared instance per command type T.
template <typename T>
CommandInterface* getCommand() {
    static T inst;     // created once, reused forever
    return &inst;
}
