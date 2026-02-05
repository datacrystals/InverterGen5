#include "MeasurementSystem.h"
#include <algorithm>
#include <cstdio>
#include <cfloat>  // For FLT_MAX

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- MeasurementChannel Implementation ---

MeasurementChannel::MeasurementChannel(const ChannelConfig& cfg) 
    : m_config(cfg) {
    m_filtered_value = cfg.offset;  // Initialize to offset
}

void MeasurementChannel::update(float adc_voltage) {
    m_last_raw_voltage = adc_voltage;
    
    float physical_value = 0.0f;
    
    switch (m_config.type) {
        case SensorType::VOLTAGE_DIVIDER:
            // V_physical = V_adc * scale (where scale is divider ratio)
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;
            
        case SensorType::BIPOLAR_CURRENT:
            // (V_adc - V_zero) * sensitivity A/V
            physical_value = (adc_voltage - m_config.zero_offset_volts) * m_config.scale;
            break;
            
        case SensorType::UNIPOLAR_CURRENT:
            // V_adc * sensitivity (for low-side shunt with unipolar amp)
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;
            
        case SensorType::TEMPERATURE:
            // Assuming NTC: scale might be degrees/volt, or implement Steinhart-Hart here
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;
            
        case SensorType::THROTTLE:
            // Normalize to 0.0-1.0 or percent
            physical_value = (adc_voltage - m_config.offset) * m_config.scale;
            physical_value = std::clamp(physical_value, 0.0f, 1.0f);
            break;
            
        case SensorType::DIRECT:
        default:
            physical_value = adc_voltage + m_config.offset;
            break;
    }
    
    // Apply low-pass filter if configured
    if (m_config.low_pass_factor < 1.0f && m_config.low_pass_factor > 0.0f) {
        m_filtered_value += (physical_value - m_filtered_value) * m_config.low_pass_factor;
    } else {
        m_filtered_value = physical_value;
    }
    
    // Fault detection (stuck at 0 or max)
    constexpr float MAX_ADC_VOLTAGE = 1.8f;
    if (adc_voltage < 0.01f || adc_voltage > (MAX_ADC_VOLTAGE - 0.01f)) {
        m_faulted = true;  // Sensor disconnected or saturated
    } else {
        m_faulted = false;
    }
}

void MeasurementChannel::calibrateZero(float samples) {
    // Note: This is stubbed. Real calibration requires accumulating samples during zero-current state.
    m_accumulator = 0.0f;
    m_sample_count = 0;
}

// --- MeasurementSystem Implementation ---

MeasurementSystem::MeasurementSystem(MAX2253x_MultiADC& adc) : m_adc(adc) {}

void MeasurementSystem::addChannel(const ChannelConfig& config) {
    auto ptr = std::make_unique<MeasurementChannel>(config);
    m_channels[config.name] = std::move(ptr);
    
    // Ensure physical map is large enough
    size_t max_dev = config.device_index + 1;
    if (m_physical_map.size() < max_dev * 4) {
        m_physical_map.resize(max_dev * 4, {0, 0});
    }
    size_t linear_idx = config.device_index * 4 + config.channel;
    if (linear_idx < m_physical_map.size()) {
        m_physical_map[linear_idx] = {config.device_index, config.channel};
    }
}

void MeasurementSystem::addChannels(const std::vector<ChannelConfig>& configs) {
    for (const auto& cfg : configs) {
        addChannel(cfg);
    }
}

void MeasurementSystem::update() {
    auto all_devices = m_adc.read_all_devices_voltage();
    
    for (auto& [name, channel] : m_channels) {
        const auto& cfg = channel->getConfig();
        if (cfg.device_index < all_devices.size() && cfg.channel < 4) {
            float voltage = all_devices[cfg.device_index][cfg.channel];
            channel->update(voltage);
        }
    }
    
    // Track encoder min/max during operation
    if (m_encoder_tracking_active) {
        auto sin_it = m_channels.find("ENCODER_SIN");
        auto cos_it = m_channels.find("ENCODER_COS");
        
        if (sin_it != m_channels.end() && cos_it != m_channels.end()) {
            float sin_val = sin_it->second->getValue();
            float cos_val = cos_it->second->getValue();
            
            // Update min/max for both channels
            if (sin_val < m_encoder_sin_min) m_encoder_sin_min = sin_val;
            if (sin_val > m_encoder_sin_max) m_encoder_sin_max = sin_val;
            if (cos_val < m_encoder_cos_min) m_encoder_cos_min = cos_val;
            if (cos_val > m_encoder_cos_max) m_encoder_cos_max = cos_val;
        }
    }
}

float MeasurementSystem::read(const std::string& channel_name) const {
    auto it = m_channels.find(channel_name);
    if (it != m_channels.end()) {
        return it->second->getValue();
    }
    return 0.0f;  // Or NAN
}

float MeasurementSystem::getDCBusVoltage() const {
    const char* names[] = {"V_DC", "V_BUS", "HV_BUS", "BATTERY", "DC_BUS"};
    for (const char* name : names) {
        auto val = read(name);
        if (val != 0.0f) return val;
    }
    return 0.0f;
}

float MeasurementSystem::getBatteryVoltage() const {
    const char* names[] = {"V_BATTERY", "V_BAT", "BATTERY"};
    for (const char* name : names) {
        auto it = m_channels.find(name);
        if (it != m_channels.end()) {
            return it->second->getValue();
        }
    }
    return 0.0f;
}

float MeasurementSystem::getPhaseCurrent(uint8_t phase) const {
    char buf[16];
    snprintf(buf, sizeof(buf), "I_PHASE_%c", 'A' + phase);
    return read(buf);
}

void MeasurementSystem::printChannels() const {
    printf("\n=== Measurement System Channels ===\n");
    printf("%-15s %-10s %-12s %-10s %-10s\n", "Name", "Device", "Type", "Raw(V)", "Physical");
    printf("---------------------------------------------------------\n");
    
    for (const auto& [name, ch] : m_channels) {
        const auto& cfg = ch->getConfig();
        const char* type_str = "UNKNOWN";
        switch(cfg.type) {
            case SensorType::VOLTAGE_DIVIDER: type_str = "HV_VOLT"; break;
            case SensorType::BIPOLAR_CURRENT: type_str = "BIP_CUR"; break;
            case SensorType::UNIPOLAR_CURRENT: type_str = "UNI_CUR"; break;
            case SensorType::TEMPERATURE: type_str = "TEMP"; break;
            case SensorType::THROTTLE: type_str = "THROTTLE"; break;
            case SensorType::DIRECT: type_str = "DIRECT"; break;
        }
        
        printf("%-15s %-4zu/%-4u %-12s %8.3f   %8.3f\n", 
               name.c_str(), cfg.device_index, cfg.channel, type_str,
               ch->getRawVoltage(), ch->getValue());
    }
    printf("\n");
}

// void MeasurementSystem::calibrateCurrentSensors() {
//     printf("Calibrating current sensors (ensure NO current flowing)...\n");
//     for (auto& [name, ch] : m_channels) {
//         if (ch->getConfig().type == SensorType::BIPOLAR_CURRENT) {
//             ch->calibrateZero();
//         }
//     }
// }

bool MeasurementSystem::isChannelFaulted(const std::string& name) const {
    auto it = m_channels.find(name);
    if (it != m_channels.end()) {
        return it->second->isFaulted();
    }
    return true; // If channel doesn't exist, consider it faulted
}

float MeasurementSystem::getRotorPositionDegrees() const {
    auto sin_it = m_channels.find("ENCODER_SIN");
    auto cos_it = m_channels.find("ENCODER_COS");
    
    if (sin_it == m_channels.end() || cos_it == m_channels.end()) {
        return NAN;
    }
    
    // Dynamic center points from tracked min/max
    float sin_center = (m_encoder_sin_min + m_encoder_sin_max) * 0.5f;
    float cos_center = (m_encoder_cos_min + m_encoder_cos_max) * 0.5f;
    
    // Remove dynamic offsets
    float sin_centered = sin_it->second->getValue() - sin_center;
    float cos_centered = cos_it->second->getValue() - cos_center;
    
    // Optional: Normalize amplitudes if channels have different gains
    float sin_amp = (m_encoder_sin_max - m_encoder_sin_min) * 0.5f;
    float cos_amp = (m_encoder_cos_max - m_encoder_cos_min) * 0.5f;
    
    // Only normalize if we have reasonable amplitude (>10mV)
    if (sin_amp > 0.01f && cos_amp > 0.01f) {
        sin_centered /= sin_amp;
        cos_centered /= cos_amp;
    }
    
    // Calculate angle (-π to π)
    float angle_rad = atan2f(sin_centered, cos_centered);
    
    // Convert to 0-360 degrees
    float angle_deg = angle_rad * 180.0f / M_PI;
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }
    
    return angle_deg;
}