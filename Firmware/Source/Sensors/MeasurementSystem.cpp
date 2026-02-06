// ========================= MeasurementSystem.cpp =========================
#include "MeasurementSystem.h"
#include <algorithm>
#include <cstdio>
#include <cfloat>

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
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;

        case SensorType::BIPOLAR_CURRENT:
            physical_value = (adc_voltage - m_config.zero_offset_volts) * m_config.scale; // scale is A/V

            break;

        case SensorType::UNIPOLAR_CURRENT:
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;

        case SensorType::TEMPERATURE:
            physical_value = adc_voltage * m_config.scale + m_config.offset;
            break;

        case SensorType::THROTTLE:
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
        m_faulted = true;
    } else {
        m_faulted = false;
    }
}

void MeasurementChannel::calibrateZero(float samples) {
    (void)samples;
    m_accumulator = 0.0f;
    m_sample_count = 0;
}

// --- MeasurementSystem Implementation ---

MeasurementSystem::MeasurementSystem(MAX2253x_MultiADC& adc) : m_adc(adc) {}

void MeasurementSystem::resetEncoderTracking() {
    m_encoder_sin_min = FLT_MAX;
    m_encoder_sin_max = -FLT_MAX;
    m_encoder_cos_min = FLT_MAX;
    m_encoder_cos_max = -FLT_MAX;

    m_encoder_tracking_initialized = false;

    m_encoder_last_sin = 0.0f;
    m_encoder_last_cos = 0.0f;
    m_encoder_still_count = 0;
    m_encoder_stationary = false;

    m_encoder_cal_locked = false;
    m_encoder_sin_center_locked = 0.0f;
    m_encoder_cos_center_locked = 0.0f;
    m_encoder_sin_amp_locked = 1.0f;
    m_encoder_cos_amp_locked = 1.0f;
}

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

    // --- Encoder tracking: stable at standstill + lock once swing is sufficient ---
    if (!m_encoder_tracking_active) {
        return;
    }

    auto sin_it = m_channels.find("ENCODER_SIN");
    auto cos_it = m_channels.find("ENCODER_COS");
    if (sin_it == m_channels.end() || cos_it == m_channels.end()) {
        return;
    }

    auto* sin_ch = sin_it->second.get();
    auto* cos_ch = cos_it->second.get();
    if (sin_ch->isFaulted() || cos_ch->isFaulted()) {
        return;
    }

    float sin_val = sin_ch->getValue();
    float cos_val = cos_ch->getValue();

    // Init on first good sample
    if (!m_encoder_tracking_initialized) {
        m_encoder_sin_min = m_encoder_sin_max = sin_val;
        m_encoder_cos_min = m_encoder_cos_max = cos_val;

        m_encoder_last_sin = sin_val;
        m_encoder_last_cos = cos_val;

        m_encoder_still_count = 0;
        m_encoder_stationary = false;

        m_encoder_cal_locked = false;
        m_encoder_tracking_initialized = true;
        return;
    }

    // Stationary detection (gap-independent)
    // NOTE: Use a tiny absolute epsilon; this works across many encoders because it is
    // near ADC noise floor. If your ADC is noisier, raise STILL_EPS.
    constexpr float STILL_EPS = 0.0008f;   // 0.8mV
    constexpr uint32_t STILL_N = 80;       // require "still" for N update() calls

    float ds = fabsf(sin_val - m_encoder_last_sin);
    float dc = fabsf(cos_val - m_encoder_last_cos);

    if ((ds + dc) < STILL_EPS) {
        if (m_encoder_still_count < 0xFFFFFFFFu) m_encoder_still_count++;
    } else {
        m_encoder_still_count = 0;
    }
    m_encoder_stationary = (m_encoder_still_count >= STILL_N);

    m_encoder_last_sin = sin_val;
    m_encoder_last_cos = cos_val;

    // Only update min/max while moving AND not locked.
    // Outward-only updates: prevents amplitude collapse at standstill.
    if (!m_encoder_stationary && !m_encoder_cal_locked) {
        if (sin_val < m_encoder_sin_min) m_encoder_sin_min = sin_val;
        if (sin_val > m_encoder_sin_max) m_encoder_sin_max = sin_val;
        if (cos_val < m_encoder_cos_min) m_encoder_cos_min = cos_val;
        if (cos_val > m_encoder_cos_max) m_encoder_cos_max = cos_val;

        // Lock calibration once swing is "enough".
        //
        // Support arbitrary sin/cos encoders:
        // - We pick a lock threshold based on ADC range, not your specific encoder.
        // - But your current encoder swing is ~0.2 to 0.665V => amplitude ~0.2325V.
        //   So a good general lock minimum is ~0.05V (50mV), well below typical swing.
        float sin_amp = (m_encoder_sin_max - m_encoder_sin_min) * 0.5f;
        float cos_amp = (m_encoder_cos_max - m_encoder_cos_min) * 0.5f;

        constexpr float AMP_LOCK_MIN = 0.05f; // 50mV (works for your swing and many others)

        if (sin_amp > AMP_LOCK_MIN && cos_amp > AMP_LOCK_MIN) {
            m_encoder_sin_center_locked = (m_encoder_sin_min + m_encoder_sin_max) * 0.5f;
            m_encoder_cos_center_locked = (m_encoder_cos_min + m_encoder_cos_max) * 0.5f;
            m_encoder_sin_amp_locked    = sin_amp;
            m_encoder_cos_amp_locked    = cos_amp;
            m_encoder_cal_locked = true;
        }
    }
}

float MeasurementSystem::read(const std::string& channel_name) const {
    auto it = m_channels.find(channel_name);
    if (it != m_channels.end()) {
        return it->second->getValue();
    }
    return 0.0f;
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
    std::snprintf(buf, sizeof(buf), "I_PHASE_%c", 'A' + phase);
    return read(buf);
}

void MeasurementSystem::printChannels() const {
    std::printf("\n=== Measurement System Channels ===\n");
    std::printf("%-15s %-10s %-12s %-10s %-10s\n", "Name", "Device", "Type", "Raw(V)", "Physical");
    std::printf("---------------------------------------------------------\n");

    for (const auto& [name, ch] : m_channels) {
        const auto& cfg = ch->getConfig();
        const char* type_str = "UNKNOWN";
        switch (cfg.type) {
            case SensorType::VOLTAGE_DIVIDER:  type_str = "HV_VOLT"; break;
            case SensorType::BIPOLAR_CURRENT:  type_str = "BIP_CUR"; break;
            case SensorType::UNIPOLAR_CURRENT: type_str = "UNI_CUR"; break;
            case SensorType::TEMPERATURE:      type_str = "TEMP"; break;
            case SensorType::THROTTLE:         type_str = "THROTTLE"; break;
            case SensorType::DIRECT:           type_str = "DIRECT"; break;
        }

        std::printf("%-15s %-4zu/%-4u %-12s %10.6f   %8.3f\n",
                    name.c_str(), cfg.device_index, cfg.channel, type_str,
                    ch->getRawVoltage(), ch->getValue());
    }
    std::printf("\n");
}

bool MeasurementSystem::isChannelFaulted(const std::string& name) const {
    auto it = m_channels.find(name);
    if (it != m_channels.end()) {
        return it->second->isFaulted();
    }
    return true;
}

float MeasurementSystem::getRotorPositionDegrees() const {
    auto sin_it = m_channels.find("ENCODER_SIN");
    auto cos_it = m_channels.find("ENCODER_COS");

    if (sin_it == m_channels.end() || cos_it == m_channels.end()) {
        return NAN;
    }

    float sin_val = sin_it->second->getValue();
    float cos_val = cos_it->second->getValue();

    // If we don't have locked calibration yet, provide a usable fallback.
    // IMPORTANT: This won't be "absolute" until locked, but it's stable and avoids FLT_MAX math.
    if (!m_encoder_tracking_initialized || !m_encoder_cal_locked) {
        float angle_rad = atan2f(sin_val, cos_val);
        float angle_deg = angle_rad * 180.0f / M_PI;
        if (angle_deg < 0.0f) angle_deg += 360.0f;
        return angle_deg;
    }

    float sin_centered = sin_val - m_encoder_sin_center_locked;
    float cos_centered = cos_val - m_encoder_cos_center_locked;

    // Normalize with locked amplitudes to handle different encoder gains.
    float sin_amp = m_encoder_sin_amp_locked;
    float cos_amp = m_encoder_cos_amp_locked;

    // Guard: never divide by tiny numbers (works across many encoder types)
    constexpr float AMP_MIN = 0.02f; // 20mV
    if (sin_amp > AMP_MIN && cos_amp > AMP_MIN) {
        sin_centered /= sin_amp;
        cos_centered /= cos_amp;
    }

    float angle_rad = atan2f(sin_centered, cos_centered);
    float angle_deg = angle_rad * 180.0f / M_PI;
    if (angle_deg < 0.0f) angle_deg += 360.0f;
    return angle_deg;
}
