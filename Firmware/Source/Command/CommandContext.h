#ifndef COMMAND_CONTEXT_H
#define COMMAND_CONTEXT_H
#include <Switching/PWMDriver.h>
#include <Hardware.h>
#include <cstdint>

// Forward declarations - sufficient for pointers
class PWMDriver;
class CommutationManager;

struct CommandContext {
    PWMDriver* driver;
    CommutationManager* zone_mgr;
    float* ramp_rate;
    float* manual_carrier_hz;
    bool* manual_carrier_mode;
    float* last_carrier_hz;
    bool* last_sync_mode;
    uint16_t* last_pulses;
    void (*update_carrier)();
};

#endif