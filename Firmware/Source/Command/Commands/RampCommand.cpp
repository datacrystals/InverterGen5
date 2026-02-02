#include "../CommandInterface.h"
#include "../CommandContext.h"
#include "Hardware.h"
#include <cstdio>

#include "Command/RtCtxHelpers.h"

class RampCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "R"; }
    const char* getShortDescription() const override { return "Set ramp rate"; }

    int getArgCount() const override { return 1; }

    ArgSpec getArgSpec(int /*index*/) const override {
        return {
            .name = "rate",
            .unit = "Hz/s",
            .min = 0.1f,
            .max = Hardware::Limits::Fundamental::MAX_RAMP_HZ_S,
            .default_val = 5.0f,
            .required = true,
            .type = ArgSpec::FLOAT
        };
    }

    void execute(const ArgValue* args, CommandContext& ctx) override {
        const float r = args[0].f_val;

        // Core0-safe: enqueue to core1
        ctx_set_ramp(ctx, r);

        printf("Ramp rate: %.2f Hz/s\r\n", r);
    }

    static RampCommand& instance() {
        static RampCommand inst;
        return inst;
    }
};

CommandInterface* getRampCommand() {
    return &RampCommand::instance();
}
