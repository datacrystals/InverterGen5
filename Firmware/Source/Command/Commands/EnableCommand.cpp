#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class EnableCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "E"; }
    const char* getShortDescription() const override { return "Enable after emergency stop"; }

    void execute(const ArgValue* /*args*/, CommandContext& ctx) override {
        RtStatus status{};
        const bool have = (ctx.try_get_status && ctx.try_get_status(&status));

        if (have) {
            if (status.estop) {
                if (ctx.clear_emergency_stop) ctx.clear_emergency_stop();
                if (ctx.enable) ctx.enable();
                printf("Re-enabled\r\n");
                return;
            }

            // Not in estop — just enable if needed
            if (!status.enabled) {
                if (ctx.enable) ctx.enable();
                printf("Enabled\r\n");
            } else {
                printf("Already enabled\r\n");
            }
            return;
        }

        // Status unavailable (very early startup)
        if (ctx.enable) ctx.enable();
        printf("Enable requested\r\n");
    }

    static EnableCommand& instance() {
        static EnableCommand inst;
        return inst;
    }
};

CommandInterface* getEnableCommand() {
    return &EnableCommand::instance();
}
