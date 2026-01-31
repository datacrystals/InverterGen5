#pragma once

#include <cstdint>
#include <cmath>

#include "Hardware.h"

/**
 * PWM Modulation Strategy Types
 * 
 * ASYNC_FIXED: Constant carrier frequency (good for low-speed, current ripple control)
 * ASYNC_RAMP:  Linear carrier sweep (smooth transition between zones)
 * SYNC:        Synchronous modulation (carrier locked to fundamental, reduces switching losses at high speed)
 */
enum class ZoneType {
    ASYNC_FIXED,  // Constant carrier frequency [Hz]
    ASYNC_RAMP,   // Linear carrier ramp between start/end frequencies
    SYNC          // Synchronous modulation: carrier = n × fundamental
};

/**
 * Zone Configuration Structure
 * Defines a frequency band with specific modulation strategy
 */
struct ZoneConfig {
    float freq_start;    // Zone start frequency [Hz]
    float freq_end;      // Zone end frequency [Hz] (exclusive)
    ZoneType type;
    float param1;        // FIXED: carrier_hz, RAMP: carrier_start_hz, SYNC: pulses_per_cycle
    float param2;        // RAMP: carrier_end_hz, others: unused
    
    // Convenience accessors
    float carrierFreq() const { return param1; }
    float rampStart() const { return param1; }
    float rampEnd() const { return param2; }
    uint16_t pulsesPerCycle() const { return static_cast<uint16_t>(param1); }
};

/**
 * CommutationManager
 * 
 * Manages hybrid PWM strategies for motor control across wide speed ranges.
 * Typical EV setup: Async at low speed (current control) → Sync at high speed (loss reduction)
 * 
 * Hardware limits set for CM600HA-24H IGBT module (600V/600A):
 * - Min carrier: 800 Hz (avoid excessive ripple at high current)
 * - Max carrier: 10 kHz (switching loss limit for this module)
 */
class CommutationManager {
private:
    static constexpr int MAX_ZONES = COMMUTATION_PATTERN_MAX_ZONES;
    static constexpr float DEFAULT_CARRIER = COMMUTATION_PATTERN_DEFAULT_HZ;
    static constexpr float MIN_CARRIER = MIN_SWITCHING_FREQUENCY_HZ;
    static constexpr float MAX_CARRIER = MAX_SWITCHING_FREQUENCY_HZ;
    
    ZoneConfig zones[MAX_ZONES];
    int zone_count = 0;

public:
    CommutationManager();
    
    // Zone Configuration
    void clearZones();
    void addAsyncFixed(float start_freq, float end_freq, float carrier_hz);
    void addAsyncRamp(float start_freq, float end_freq, float carrier_start, float carrier_end);
    void addSync(float start_freq, float end_freq, uint16_t pulses_per_cycle);
    
    // Runtime Methods
    bool getZone(float freq, ZoneConfig* zone) const;
    float calculateCarrier(float freq, const ZoneConfig* zone, float* sync_pulses = nullptr) const;
    
    // Utility
    int getZoneCount() const { return zone_count; }
    bool isConfigured() const { return zone_count > 0; }
    
    // Convenience wrapper
    float getCarrierForFrequency(float freq, float* sync_pulses = nullptr) const;
};