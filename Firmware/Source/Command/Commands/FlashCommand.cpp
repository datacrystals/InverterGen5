#include "pico/bootrom.h"

#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <cstdio>

class FlashCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "flash"; }
    const char* getShortDescription() const override { return "Reboot system in flasher mode"; }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        reset_usb_boot(0, 0);
    }
    
    static FlashCommand& instance() {
        static FlashCommand inst;
        return inst;
    }
};

CommandInterface* getFlashCommand() {
    return &FlashCommand::instance();
}