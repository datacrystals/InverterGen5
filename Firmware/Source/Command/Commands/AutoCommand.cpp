#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

#include "Command/RtCtxHelpers.h"

class AutoCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "A"; }
    const char* getShortDescription() const override { return "Return to automatic zone-based carrier"; }

    void execute(const ArgValue* /*args*/, CommandContext& ctx) override {
        // Core0-safe: core1 will resume zone-based carrier selection
        ctx_set_manual_carrier(ctx, false);

        printf("AUTO mode: Zone-based carrier active\r\n");
    }

    static AutoCommand& instance() {
        static AutoCommand inst;
        return inst;
    }
};

CommandInterface* getAutoCommand() {
    return &AutoCommand::instance();
}
