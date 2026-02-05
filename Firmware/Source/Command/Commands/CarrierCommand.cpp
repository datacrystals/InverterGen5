#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <Hardware.h>
#include <cstdio>

class CarrierCommand : public CommandInterface {
public:
 CarrierCommand()
      : CommandInterface("EN", "Enable output",
            ArgSpec{ .name = "freq",
            .unit = "Hz",
            .min = Hardware::Limits::Switching::MIN_HZ,   // 100
            .max = Hardware::Limits::Switching::MAX_HZ,   // 10000
            .default_val = 2000,
            .required = true,
            .type = ArgSpec::FLOAT})
    {}

    void execute(const ArgValue* args, CommandContext& ctx) override {
        float carrier = args[0].f_val;

        // Core0 -> Core1 messages
        ctx.set_manual_carrier_hz(carrier);
        ctx.set_manual_carrier_mode(true);

        // No direct carrier update on core0
        printf("Manual carrier: %.1f Hz (AUTO mode OFF)\r\n", carrier);
    }
};

CommandInterface* makeCarrierCommand() {
    static CarrierCommand inst;
    return &inst;
}
