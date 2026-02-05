// CmdAsyncFixed.h
#pragma once

#include "../CommandInterface.h"
#include <Hardware.h>
#include <Switching/CommutationManager.h>

// Replace `ctx.commutationMgr` with however your CommandContext exposes CommutationManager.
class AsyncCommand : public CommandInterface {
public:
    AsyncCommand()
      : CommandInterface(
            "AFIX",
            "Add an ASYNC_FIXED commutation zone (fixed carrier)",
            {
                ArgSpec{ "start", "Hz",
                         0,
                         Hardware::Limits::Switching::MAX_HZ,
                         0.0f, true, ArgSpec::FLOAT },

                ArgSpec{ "end", "Hz",
                         0,
                         Hardware::Limits::Switching::MAX_HZ,
                         0.0f, true, ArgSpec::FLOAT },

                ArgSpec{ "carrier", "Hz",
                         Hardware::Limits::Switching::MIN_HZ,
                         Hardware::Limits::Switching::MAX_HZ,
                         Hardware::Commutation::DEFAULT_HZ, true, ArgSpec::FLOAT },
            }
        )
    {}

    void execute(const ArgValue* args, CommandContext& ctx) override {
        const float start_hz   = args[0].f_val;
        const float end_hz     = args[1].f_val;
        const float carrier_hz = args[2].f_val;

        // zone manager is bridged through ctx.zone_mgr (set in RtBridge::initAndGetContext)
        if (!ctx.zone_mgr) {
            return;
        }
        ctx.zone_mgr->clearZones();
        ctx.zone_mgr->addAsyncFixed(start_hz, end_hz, carrier_hz);

        // Optional: if you want manual carrier mode OFF so zones take effect immediately:
        // if (ctx.set_manual_carrier_mode) ctx.set_manual_carrier_mode(false);

        // Optional: status/ack
        // if (ctx.println) ctx.println("OK");
    }
    
};

CommandInterface* makeAsyncCommand() {
    static AsyncCommand inst;
    return &inst;
}