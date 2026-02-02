#include "../CommandInterface.h"
#include "../CommandContext.h"
#include "Hardware.h"
#include <cstdio>

class ImmediateCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "I"; }
    const char* getShortDescription() const override { return "Set frequency immediately (no ramp)"; }

    int getArgCount() const override { return 1; }

    ArgSpec getArgSpec(int /*index*/) const override {
        return {
            .name = "freq",
            .unit = "Hz",
            .min = -200.0f,
            .max = 200.0f,
            .default_val = 0.0f,
            .required = true,
            .type = ArgSpec::FLOAT
        };
    }

    void execute(const ArgValue* args, CommandContext& ctx) override {
        const float f = args[0].f_val;

        RtStatus status{};
        const bool have_status = (ctx.try_get_status && ctx.try_get_status(&status));

        if (have_status && status.estop) {
            printf("Error: Emergency stop active\r\n");
            return;
        }

        if (ctx.set_frequency_immediate) {
            ctx.set_frequency_immediate(f);
        } else if (ctx.set_target_frequency) {
            // Fallback: if immediate hook not provided, at least set target
            ctx.set_target_frequency(f);
        } else {
            printf("Error: No frequency control hook available\r\n");
            return;
        }

        // Auto-enable if nonzero requested
        if (f != 0.0f && ctx.enable) {
            if (!have_status || !status.enabled) {
                ctx.enable();
            }
        }

        printf("Immediate: %.2f Hz\r\n", f);
    }

    static ImmediateCommand& instance() {
        static ImmediateCommand inst;
        return inst;
    }
};

CommandInterface* getImmediateCommand() {
    return &ImmediateCommand::instance();
}
