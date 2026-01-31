#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include "CommandInterface.h"
#include "CommandContext.h"
#include <array>
#include <cstddef>
#include <cstdint>

class CommandManager {
public:
    static CommandManager& instance();
    
    void registerCommand(CommandInterface* cmd);
    void setContext(CommandContext& ctx);  // Declaration only
    
    // Parses line, extracts first token, matches case-insensitively
    void processLine(const char* line);
    
    void printHelp() const;
    
private:
    CommandManager() = default;
    
    // Case-insensitive string search
    CommandInterface* findCommand(const char* name);  // Added this declaration
    
    bool nameEquals(const char* a, const char* b);
    
    static constexpr size_t MAX_CMDS = 20;
    std::array<CommandInterface*, MAX_CMDS> commands_;
    size_t count_ = 0;
    CommandContext* context_ = nullptr;
};

#endif