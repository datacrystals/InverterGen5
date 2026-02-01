// MAX2253x.h (add friend declaration)
#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include "pico/stdlib.h"
#include "hardware/spi.h"

enum class ErrorCode {
    NONE = 0,
    NOT_INITIALIZED,
    WRONG_DEVICE_ID,
    ADC_FUNCTIONAL_FAULT,
    FIELD_DATA_LOSS,
    SPI_FRAMING_ERROR,
    SPI_CRC_ERROR,
    ADC_ALL_ZEROS,
    ADC_ALL_MAX,
    TIMEOUT
};

class MAX2253x_Device {
public:
    explicit MAX2253x_Device(uint8_t cs_pin);
    
    bool init();
    bool is_initialized() const { return m_initialized; }
    
    bool verify_chip_id();
    bool check_diagnostics();
    bool verify_adc_reading();
    
    std::array<uint16_t, 4> read_all_adc_raw();
    std::array<float, 4> read_all_adc_voltage();
    
    uint8_t get_cs_pin() const { return m_cs_pin; }
    ErrorCode get_last_error() const { return m_last_error; }
    const char* get_error_string() const;
    uint8_t get_device_id() const { return m_device_id; }

private:
    uint8_t m_cs_pin;
    bool m_initialized;
    uint8_t m_device_id;
    ErrorCode m_last_error;
    
    void begin_transaction();
    void end_transaction();
    uint16_t read_register(uint8_t address);
    
    // Allow MultiADC to access private members for initialization/diagnostics
    friend class MAX2253x_MultiADC;
};

class MAX2253x_MultiADC {
public:
    static spi_inst_t* SPI_PORT;
    static constexpr uint32_t SPI_BAUDRATE = 200'000;
    
    explicit MAX2253x_MultiADC(const std::vector<uint8_t>& cs_pins);
    
    bool init();
    void print_status();
    std::array<uint16_t, 4> read_device_raw(size_t device_index);
    std::array<float, 4> read_device_voltage(size_t index);
    std::vector<std::array<uint16_t, 4>> read_all_devices_raw();
    std::vector<std::array<float, 4>> read_all_devices_voltage();
    size_t get_device_count() const { return m_devices.size(); }

private:
    std::vector<MAX2253x_Device> m_devices;
};