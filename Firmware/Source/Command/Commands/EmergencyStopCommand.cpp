#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>
#include <Switching/PWMDriver.h>
#include <Hardware.h>

class EmergencyStopCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "STOP"; }
    const char* getShortDescription() const override { return "Emergency stop (immediate)"; }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        if (ctx.driver) {
            ctx.driver->emergencyStop();
            printf("EMERGENCY STOP\r\n");
        }
    }
    
    static EmergencyStopCommand& instance() {
        static EmergencyStopCommand inst;
        return inst;
    }
};

CommandInterface* getEmergencyStopCommand() {
    return &EmergencyStopCommand::instance();
}