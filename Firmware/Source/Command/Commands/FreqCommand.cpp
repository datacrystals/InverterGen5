#include "../CommandInterface.h"
#include "../CommandContext.h"
#include <PWMDriver.h>
#include <Hardware.h>
#include <cstdio>

class FreqCommand : public CommandInterface {
public:
    const char* getCommandName() const override { return "F"; }
    const char* getShortDescription() const override { return "Set output frequency"; }
    
    int getArgCount() const override { return 1; }
    
    ArgSpec getArgSpec(int index) const override {
        return {
            .name = "freq",
            .unit = "Hz",
            .min = MIN_FUNDAMENTAL_FREQUENCY_HZ,  // -500 or 0
            .max = MAX_FUNDAMENTAL_FREQUENCY_HZ,  // 500
            .default_val = 0,
            .required = true,
            .type = ArgSpec::FLOAT
        };
    }
    
    void execute(const ArgValue* args, CommandContext& ctx) override {
        // args[0] is guaranteed to be present and within bounds
        float f = args[0].f_val;
        
        if (ctx.driver->isEmergencyStopped()) {
            printf("Error: Emergency stop active, press E to enable\r\n");
            return;
        }
        
        ctx.driver->setTargetFrequency(f, *ctx.ramp_rate);
        if (f != 0.0f && !ctx.driver->isEnabled()) 
            ctx.driver->enable();
        printf("Target freq: %.2f Hz\r\n", f);
    }
    
    static FreqCommand& instance() {
        static FreqCommand inst;
        return inst;
    }
};

CommandInterface* getFreqCommand() {
    return &FreqCommand::instance();
}