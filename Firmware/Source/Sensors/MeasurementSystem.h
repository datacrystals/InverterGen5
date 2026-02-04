#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cmath>
#include <cfloat>  // For FLT_MAX
#include "MAX2253x.h"

enum class SensorType {
    VOLTAGE_DIVIDER,      // HV bus, battery voltage (unipolar)
    BIPOLAR_CURRENT,      // Shunt with offset (e.g., 0.5V = 0A, 2.5V = +Imax)
    UNIPOLAR_CURRENT,     // Shunt single-ended (0V = 0A)
    TEMPERATURE,          // NTC or analog temp sensor
    THROTTLE,             // 0-5V or 0-3.3V potentiometer
    DIRECT                // Raw 0-1.8V, no scaling
};

struct ChannelConfig {
    size_t device_index;      // Which MAX2253x chip (0, 1, 2...)
    uint8_t channel;          // Which channel on that chip (0-3)
    SensorType type;
    float scale;              // Multiplier (V/divider_ratio or A/V_sensitivity)
    float offset;             // Additive offset (volts or amps)
    float low_pass_factor;    // 0.0-1.0 for IIR filter (1.0 = no filter)
    std::string name;         // Human readable name
    
    // For current sensors: what represents zero current (typically 0.5 or 0.9V for isolated amps)
    float zero_offset_volts;  
};

class MeasurementChannel {
public:
    MeasurementChannel(const ChannelConfig& cfg);
    
    // Update with new raw ADC voltage reading (0-1.8V for MAX2253x)
    void update(float adc_voltage);
    
    // Get physical value (volts, amps, degrees, etc)
    float getValue() const { return m_filtered_value; }
    float getRawVoltage() const { return m_last_raw_voltage; }
    uint16_t getRawADC() const { return m_last_raw_adc; }
    
    // Zero calibration for current sensors
    void calibrateZero(float samples = 100.0f);
    
    const std::string& getName() const { return m_config.name; }
    bool isFaulted() const { return m_faulted; }
    const ChannelConfig& getConfig() const { return m_config; }

private:
    ChannelConfig m_config;
    float m_last_raw_voltage = 0.0f;
    uint16_t m_last_raw_adc = 0;
    float m_filtered_value = 0.0f;
    float m_accumulator = 0.0f;  // For calibration
    uint32_t m_sample_count = 0;
    bool m_faulted = false;
};

class MeasurementSystem {
public:
    explicit MeasurementSystem(MAX2253x_MultiADC& adc);
    
    // Register channels - call this once at startup
    void addChannel(const ChannelConfig& config);
    void addChannels(const std::vector<ChannelConfig>& configs);
    
    // Read physical values
    float read(const std::string& channel_name) const;
    float read(size_t device_idx, uint8_t channel) const;
    
    // Convenience accessors for common EV inverter signals
    float getDCBusVoltage() const;
    float getBatteryVoltage() const;
    float getPhaseCurrent(uint8_t phase) const;  // 0=A, 1=B, 2=C
    float getThrottle() const;
    float getIGBTTemperature(uint8_t idx = 0) const;
    
    // Batch update - call in your main loop
    void update();
    
    // Diagnostics
    void printChannels() const;
    void calibrateCurrentSensors();  // Zero all bipolar current channels
    bool isChannelFaulted(const std::string& name) const;

    // --- Sin/Cos Encoder Methods ---
    // Dynamic tracking (no stationary requirement)
    void startEncoderTracking();           // Begin tracking min/max
    void stopEncoderTracking();            // Stop tracking
    void resetEncoderTracking();           // Reset min/max values
    bool isEncoderTracking() const;        // Check if tracking active
    float getRotorPositionDegrees() const; // Get angle (0-360°)

    // --- Encoder Tracking State (dynamic centering) ---
    float m_encoder_sin_min = FLT_MAX;
    float m_encoder_sin_max = -FLT_MAX;
    float m_encoder_cos_min = FLT_MAX;
    float m_encoder_cos_max = -FLT_MAX;
    bool m_encoder_tracking_active = false;

private:
    MAX2253x_MultiADC& m_adc;
    std::unordered_map<std::string, std::unique_ptr<MeasurementChannel>> m_channels;
    std::vector<std::pair<size_t, uint8_t>> m_physical_map;  // Reverse lookup


};