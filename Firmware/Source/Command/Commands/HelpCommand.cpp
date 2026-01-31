#include "../CommandInterface.h"
#include "../CommandManager.h"
#include <cstdio>

class HelpCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "HELP"; }
    const char* getShortDescription() const override { return "Show this help message"; }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        CommandManager::instance().printHelp();
    }
    
    static HelpCommand& instance() {
        static HelpCommand inst;
        return inst;
    }
};

CommandInterface* getHelpCommand() {
    return &HelpCommand::instance();
}