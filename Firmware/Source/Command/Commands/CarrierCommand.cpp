#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <Hardware.h>
#include <cstdio>

class CarrierCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "C"; }
    const char* getShortDescription() const override {
        return "Set manual carrier frequency";
    }

    int getArgCount() const override { return 1; }

    ArgSpec getArgSpec(int index) const override {
        return {
            .name = "freq",
            .unit = "Hz",
            .min = Hardware::Limits::Switching::MIN_HZ,   // 100
            .max = Hardware::Limits::Switching::MAX_HZ,   // 10000
            .default_val = 2000,
            .required = true,
            .type = ArgSpec::FLOAT
        };
    }

    void execute(const ArgValue* args, CommandContext& ctx) override {
        float carrier = args[0].f_val;

        // Core0 -> Core1 messages
        ctx.set_manual_carrier_hz(carrier);
        ctx.set_manual_carrier_mode(true);

        // No direct carrier update on core0
        printf("Manual carrier: %.1f Hz (AUTO mode OFF)\r\n", carrier);
    }

    static CarrierCommand& instance() {
        static CarrierCommand inst;
        return inst;
    }
};

CommandInterface* getCarrierCommand() {
    return &CarrierCommand::instance();
}
