#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class EmergencyStopCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "STOP"; }
    const char* getShortDescription() const override { return "Emergency stop (immediate)"; }

    void execute(const ArgValue* /*args*/, CommandContext& ctx) override {
        if (ctx.emergency_stop) {
            ctx.emergency_stop();
            printf("EMERGENCY STOP\r\n");
        } else {
            printf("Error: No emergency stop hook available\r\n");
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
