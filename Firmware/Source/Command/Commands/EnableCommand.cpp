#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class EnableCommand : public CommandInterface {
public:
    EnableCommand() : CommandInterface("E", "Enable after emergency stop") {}

    void execute(const ArgValue*, CommandContext& ctx) override {
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
};

CommandInterface* makeEnableCommand() {
    static EnableCommand inst;
    return &inst;
}
