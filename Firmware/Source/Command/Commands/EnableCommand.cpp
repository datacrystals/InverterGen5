#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <PWMDriver.h>
#include <Hardware.h>
#include <cstdio>

class EnableCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "E"; }
    const char* getShortDescription() const override { return "Enable after emergency stop"; }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        if (!ctx.driver) return;
        if (ctx.driver->isEmergencyStopped()) {
            ctx.driver->clearEmergency();
            printf("Re-enabled\r\n");
        } else {
            printf("Already enabled\r\n");
        }
    }
    
    static EnableCommand& instance() {
        static EnableCommand inst;
        return inst;
    }
};

CommandInterface* getEnableCommand() {
    return &EnableCommand::instance();
}