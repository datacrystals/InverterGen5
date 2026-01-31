#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <PWMDriver.h>
#include <Hardware.h>
#include <cstdio>

class ImmediateCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "I"; }
    const char* getShortDescription() const override { return "Set frequency immediately (no ramp)"; }
    
    int getArgCount() const override { return 1; }
    
    ArgSpec getArgSpec(int index) const override {
        return {
            .name = "freq",
            .unit = "Hz",
            .min = -200.0f,
            .max = 200.0f,
            .default_val = 0,
            .required = true,
            .type = ArgSpec::FLOAT
        };
    }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        float f = args[0].f_val;
        
        if (ctx.driver->isEmergencyStopped()) {
            printf("Error: Emergency stop active\r\n");
            return;
        }
        
        ctx.driver->setFrequencyImmediate(f);
        if (f != 0.0f && !ctx.driver->isEnabled()) 
            ctx.driver->enable();
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