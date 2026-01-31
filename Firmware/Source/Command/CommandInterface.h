#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

#include "CommandTypes.h"
#include "CommandContext.h"

class CommandInterface {
public:
    virtual ~CommandInterface() = default;
    
    // Command name (e.g., "F", "STOP", "CARRIER")
    virtual const char* getCommandName() const = 0;
    
    // Short description text (without signature/ranges)
    virtual const char* getShortDescription() const = 0;
    
    // Argument metadata - override if you take arguments
    virtual int getArgCount() const { return 0; }
    virtual ArgSpec getArgSpec(int index) const { 
        return {}; 
    }
    
    // Execute with PRE-VALIDATED arguments
    // CommandManager guarantees args[] matches getArgCount() and all bounds checked
    virtual void execute(const ArgValue* args, CommandContext& ctx) = 0;
};

#endif