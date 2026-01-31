#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class AutoCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "A"; }
    const char* getShortDescription() const override { return "Return to automatic zone-based carrier"; }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        *ctx.manual_carrier_mode = false;
        if (ctx.update_carrier) ctx.update_carrier();
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