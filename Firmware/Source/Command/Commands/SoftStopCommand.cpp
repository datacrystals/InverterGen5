#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class SoftStopCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "S"; }
    const char* getShortDescription() const override { return "Soft stop (ramp to zero)"; }
    
    // No args - leave getArgCount() at default 0
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        if (!ctx.driver) return;
        if (!ctx.driver->isEmergencyStopped()) {
            ctx.driver->setTargetFrequency(0.0f, *ctx.ramp_rate);
            printf("Soft stop (ramp to 0)\r\n");
        }
    }
    
    static SoftStopCommand& instance() {
        static SoftStopCommand inst;
        return inst;
    }
};

CommandInterface* getSoftStopCommand() {
    return &SoftStopCommand::instance();
}