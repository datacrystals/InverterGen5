#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class SoftStopCommand : public CommandInterface {
public:
    SoftStopCommand() : CommandInterface("S", "Soft stop (ramp to zero)") {}

    void execute(const ArgValue*, CommandContext& ctx) override {
        RtStatus status{};
        const bool have_status = (ctx.try_get_status && ctx.try_get_status(&status));

        // Respect emergency stop
        if (have_status && status.estop) {
            printf("Error: Emergency stop active, cannot ramp\r\n");
            return;
        }

        // Core0 -> Core1: set target frequency 0
        if (ctx.set_target_frequency) {
            ctx.set_target_frequency(0.0f);
            printf("Soft stop (ramp to 0)\r\n");
        } else if (ctx.set_frequency_immediate) {
            // Fallback if ramp hook isn't provided
            ctx.set_frequency_immediate(0.0f);
            printf("Soft stop (immediate to 0)\r\n");
        } else {
            printf("Error: No frequency control hook available\r\n");
        }
    }
};

CommandInterface* makeSoftStopCommand() {
    static SoftStopCommand inst;
    return &inst;
}