#include "CommandManager.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <strings.h>  // for strcasecmp

CommandManager& CommandManager::instance() {
    static CommandManager inst;
    return inst;
}

void CommandManager::registerCommand(CommandInterface* cmd) {
    if (count_ < MAX_CMDS && cmd != nullptr) {
        commands_[count_++] = cmd;
    }
}

// Now properly matches the declaration in header
void CommandManager::setContext(CommandContext& ctx) {
    context_ = &ctx;
}

bool CommandManager::nameEquals(const char* a, const char* b) {
    return strcasecmp(a, b) == 0;
}

// This was missing from the header before
CommandInterface* CommandManager::findCommand(const char* name) {
    for (size_t i = 0; i < count_; i++) {
        if (nameEquals(name, commands_[i]->getCommandName())) {
            return commands_[i];
        }
    }
    return nullptr;
}

// Parse next token from string, returns ptr after token
static const char* nextToken(const char* str, char* token, size_t tokenSize) {
    // Skip leading whitespace
    while (*str && isspace(*str)) str++;
    
    if (*str == '\0') {
        token[0] = '\0';
        return str;
    }
    
    // Find end of token (whitespace or null)
    const char* end = str;
    while (*end && !isspace(*end)) end++;
    
    size_t len = end - str;
    if (len >= tokenSize) len = tokenSize - 1;
    memcpy(token, str, len);
    token[len] = '\0';
    
    return end;
}

void CommandManager::processLine(const char* line) {
    if (!line || !context_) return;
    
    // Skip leading whitespace
    while (*line && isspace(*line)) line++;
    if (*line == '\0') return;
    
    // Extract command name
    char cmdName[16];
    const char* rest = nextToken(line, cmdName, sizeof(cmdName));
    
    CommandInterface* cmd = findCommand(cmdName);
    if (!cmd) {
        printf("Unknown command '%s'. Type HELP for list.\r\n", cmdName);
        return;
    }
    
    int argc = cmd->getArgCount();
    ArgValue values[4];  // Max 4 args supported
    
    // Parse and validate each expected argument
    for (int i = 0; i < argc; i++) {
        ArgSpec spec = cmd->getArgSpec(i);
        char token[32];
        rest = nextToken(rest, token, sizeof(token));
        
        // Check if argument provided
        if (token[0] == '\0') {
            if (spec.required) {
                printf("Error: Missing required argument <%s>\r\n", spec.name);
                return;
            } else {
                values[i] = {spec.default_val, (int)spec.default_val, false};
                continue;
            }
        }
        
        // Parse value
        float val;
        if (spec.type == ArgSpec::FLOAT) {
            val = atof(token);
        } else {
            val = atoi(token);
        }
        
        // Check bounds
        if (val < spec.min || val > spec.max) {
            char rangeStr[32];
            spec.printRange(rangeStr, sizeof(rangeStr));
            printf("Error: %s out of range (%s)\r\n", spec.name, rangeStr);
            return;
        }
        
        values[i] = {val, (int)val, true};
    }
    
    // Check for extra arguments
    char extra[16];
    rest = nextToken(rest, extra, sizeof(extra));
    if (extra[0] != '\0') {
        printf("Error: Too many arguments. Expected %d, got more.\r\n", argc);
        return;
    }
    
    // Execute with validated arguments
    cmd->execute(values, *context_);
}

void CommandManager::printHelp() const {
    printf("\r\n=== Command Reference ===\r\n");
    
    for (size_t i = 0; i < count_; i++) {
        CommandInterface* cmd = commands_[i];
        
        // Print command name and signature
        printf("  %-8s", cmd->getCommandName());
        
        int argc = cmd->getArgCount();
        char sigBuffer[64] = "";
        char* p = sigBuffer;
        size_t remaining = sizeof(sigBuffer);
        
        for (int j = 0; j < argc; j++) {
            ArgSpec spec = cmd->getArgSpec(j);
            int n;
            if (spec.required) {
                n = snprintf(p, remaining, " <%s>", spec.name);
            } else {
                n = snprintf(p, remaining, " [%s]", spec.name);
            }
            p += n;
            remaining -= n;
        }
        printf("%-20s - %s", sigBuffer, cmd->getShortDescription());
        
        // Print ranges if any arguments
        if (argc > 0) {
            printf("\r\n         ");
            for (int j = 0; j < argc; j++) {
                ArgSpec spec = cmd->getArgSpec(j);
                char range[32];
                spec.printRange(range, sizeof(range));
                printf(" %s:%s", spec.name, range);
            }
        }
        printf("\r\n");
    }
    printf("=========================\r\n");
}