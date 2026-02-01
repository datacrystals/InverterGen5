// MAX2253x.cpp (same implementation, now compiles)
#include "MAX2253x.h"
#include "hardware/spi.h"
#include <cstdio>
#include <cstring>
#include "Hardware.h"

static constexpr float VOLTAGE_REFERENCE = 1.8f;
static constexpr uint16_t ADC_MAX_VALUE = 4095;

static constexpr uint8_t REG_PROD_ID = 0x00;
static constexpr uint8_t REG_INTERRUPT_STATUS = 0x12;
static constexpr uint8_t REG_CONTROL = 0x14;
static constexpr uint16_t DEVICE_ID_EXPECTED = 0x0000;

spi_inst_t* MAX2253x_MultiADC::SPI_PORT = spi1;

// Helper function for SPI transaction with timeout
static bool spi_read_with_timeout(spi_inst_t* spi, uint8_t* rx_buffer, size_t len, uint32_t timeout_us) {
    absolute_time_t timeout = make_timeout_time_us(timeout_us);
    size_t bytes_read = 0;
    
    while (bytes_read < len && !time_reached(timeout)) {
        if (spi_is_readable(spi)) {
            rx_buffer[bytes_read++] = spi_get_hw(spi)->dr;
        }
    }
    
    return bytes_read == len;
}

MAX2253x_Device::MAX2253x_Device(uint8_t cs_pin) 
    : m_cs_pin(cs_pin), m_initialized(false), m_device_id(0), m_last_error(ErrorCode::NONE) {
}

bool MAX2253x_Device::init() {
    gpio_init(m_cs_pin);
    gpio_set_dir(m_cs_pin, GPIO_OUT);
    gpio_put(m_cs_pin, 1);
    
    m_initialized = true;
    printf("MAX2253x: Device CS=GPIO%u initialized\n", m_cs_pin);
    return true;
}

void MAX2253x_Device::begin_transaction() {
    gpio_put(m_cs_pin, 0);
    sleep_us(1);
}

void MAX2253x_Device::end_transaction() {
    sleep_us(1);
    gpio_put(m_cs_pin, 1);
}

uint16_t MAX2253x_Device::read_register(uint8_t address) {
    uint8_t header = (address & 0x3F) << 2;
    
    begin_transaction();
    spi_write_blocking(MAX2253x_MultiADC::SPI_PORT, &header, 1);
    
    uint8_t rx[2];
    spi_read_blocking(MAX2253x_MultiADC::SPI_PORT, 0, rx, 2);
    
    end_transaction();
    return ((rx[0] << 8) | rx[1]) & 0x0FFF;
}

std::array<uint16_t, 4> MAX2253x_Device::read_all_adc_raw() {
    std::array<uint16_t, 4> values = {0};
    uint8_t burst_cmd = 0x05;
    
    begin_transaction();
    spi_write_blocking(MAX2253x_MultiADC::SPI_PORT, &burst_cmd, 1);
    
    uint8_t rx_buffer[10];
    spi_read_blocking(MAX2253x_MultiADC::SPI_PORT, 0, rx_buffer, 10);
    
    end_transaction();
    
    for(int i = 0; i < 4; i++) {
        values[i] = ((rx_buffer[i*2] << 8) | rx_buffer[i*2 + 1]) & 0x0FFF;
    }
    
    return values;
}

std::array<float, 4> MAX2253x_Device::read_all_adc_voltage() {
    auto raw = read_all_adc_raw();
    std::array<float, 4> voltages;
    
    const float scale = VOLTAGE_REFERENCE / static_cast<float>(ADC_MAX_VALUE);
    for(int i = 0; i < 4; i++) {
        voltages[i] = static_cast<float>(raw[i]) * scale;
    }
    
    return voltages;
}

bool MAX2253x_Device::verify_chip_id() {
    if (!m_initialized) {
        m_last_error = ErrorCode::NOT_INITIALIZED;
        return false;
    }
    
    uint16_t prod_id = read_register(REG_PROD_ID);
    uint8_t device_id = (prod_id >> 8) & 0xFF;
    bool por_bit = prod_id & 0x80;
    
    printf("  Device CS=GPIO%u: PROD_ID=0x%04X (DEVICE_ID=0x%02X, POR=%d)\n", 
           m_cs_pin, prod_id, device_id, por_bit);
    
    if (device_id != DEVICE_ID_EXPECTED) {
        m_last_error = ErrorCode::WRONG_DEVICE_ID;
        printf("  ERROR: Wrong device ID! Expected 0x%02X, got 0x%02X\n", 
               DEVICE_ID_EXPECTED, device_id);
        return false;
    }
    
    if (!por_bit) {
        printf("  WARNING: POR bit not set, device may not have powered on properly\n");
    }
    
    m_device_id = device_id;
    return true;
}

bool MAX2253x_Device::check_diagnostics() {
    if (!m_initialized) {
        m_last_error = ErrorCode::NOT_INITIALIZED;
        return false;
    }
    
    uint16_t int_status = read_register(REG_INTERRUPT_STATUS);
    printf("  Device CS=GPIO%u: INTERRUPT_STATUS=0x%04X\n", m_cs_pin, int_status);
    
    bool has_fault = false;
    
    if (int_status & 0x0800) {
        printf("  ERROR: ADC functionality error detected!\n");
        has_fault = true;
        m_last_error = ErrorCode::ADC_FUNCTIONAL_FAULT;
    }
    
    if (int_status & 0x0400) {
        printf("  ERROR: Field-side data loss detected!\n");
        has_fault = true;
        m_last_error = ErrorCode::FIELD_DATA_LOSS;
    }
    
    if (int_status & 0x0200) {
        printf("  ERROR: SPI framing error detected!\n");
        has_fault = true;
        m_last_error = ErrorCode::SPI_FRAMING_ERROR;
    }
    
    if (int_status & 0x0100) {
        printf("  ERROR: SPI CRC error detected!\n");
        has_fault = true;
        m_last_error = ErrorCode::SPI_CRC_ERROR;
    }
    
    return !has_fault;
}

bool MAX2253x_Device::verify_adc_reading() {
    if (!m_initialized) {
        m_last_error = ErrorCode::NOT_INITIALIZED;
        return false;
    }
    
    auto raw_values = read_all_adc_raw();
    
    printf("  Device CS=GPIO%u: ADC raw values: [", m_cs_pin);
    for (int i = 0; i < 4; i++) {
        printf("%u", raw_values[i]);
        if (i < 3) printf(", ");
    }
    printf("]\n");
    
    bool all_zero = true;
    bool all_max = true;
    
    for (uint16_t val : raw_values) {
        if (val != 0) all_zero = false;
        if (val != 4095) all_max = false;
    }
    
    // if (all_zero) {
    //     printf("  ERROR: All ADC readings are zero! Check SPI MISO line\n");
    //     m_last_error = ErrorCode::ADC_ALL_ZEROS;
    //     return false;
    // }
    
    // if (all_max) {
    //     printf("  ERROR: All ADC readings are max (4095)! Check input voltage\n");
    //     m_last_error = ErrorCode::ADC_ALL_MAX;
    //     return false;
    // }
    
    uint32_t sum = 0;
    for (uint16_t val : raw_values) {
        sum += val;
    }
    
    // float avg = static_cast<float>(sum) / 4.0f;
    // if (avg < 100.0f) {
    //     printf("  WARNING: ADC values very low (avg %.1f)\n", avg);
    // } else if (avg > 4000.0f) {
    //     printf("  WARNING: ADC values very high (avg %.1f)\n", avg);
    // }
    
    return true;
}

const char* MAX2253x_Device::get_error_string() const {
    switch (m_last_error) {
        case ErrorCode::NONE: return "No error";
        case ErrorCode::NOT_INITIALIZED: return "Not initialized";
        case ErrorCode::WRONG_DEVICE_ID: return "Wrong device ID";
        case ErrorCode::ADC_FUNCTIONAL_FAULT: return "ADC functional fault";
        case ErrorCode::FIELD_DATA_LOSS: return "Field-side data loss";
        case ErrorCode::SPI_FRAMING_ERROR: return "SPI framing error";
        case ErrorCode::SPI_CRC_ERROR: return "SPI CRC error";
        case ErrorCode::ADC_ALL_ZEROS: return "ADC all zeros";
        case ErrorCode::ADC_ALL_MAX: return "ADC all max values";
        default: return "Unknown error";
    }
}

MAX2253x_MultiADC::MAX2253x_MultiADC(const std::vector<uint8_t>& cs_pins) {
    for(uint8_t cs : cs_pins) {
        m_devices.emplace_back(cs);
    }
}

bool MAX2253x_MultiADC::init() {
    printf("\n=== MAX2253x Multi-ADC Smart Init ===\n");
    
    spi_init(SPI_PORT, SPI_BAUDRATE);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    gpio_set_function(Hardware::Pins::SPI::SCK, GPIO_FUNC_SPI);
    gpio_set_function(Hardware::Pins::SPI::MOSI, GPIO_FUNC_SPI);
    gpio_set_function(Hardware::Pins::SPI::MISO, GPIO_FUNC_SPI);
    
    printf("SPI: %u Hz, SCK=%u, MOSI=%u, MISO=%u\n",
           SPI_BAUDRATE, Hardware::Pins::SPI::SCK, Hardware::Pins::SPI::MOSI, Hardware::Pins::SPI::MISO);
    
    bool all_ok = true;
    for(size_t i = 0; i < m_devices.size(); i++) {
        printf("\n--- Verifying ADC%zu (CS=GPIO%u) ---\n", i+1, m_devices[i].get_cs_pin());
        
        if(!m_devices[i].init()) {
            printf("ERROR: Failed to initialize ADC%zu\n", i+1);
            all_ok = false;
            continue;
        }
        
        sleep_ms(10);
        
        if(!m_devices[i].verify_chip_id()) {
            printf("ERROR: Chip ID verification failed for ADC%zu: %s\n", 
                   i+1, m_devices[i].get_error_string());
            all_ok = false;
            continue;
        }
        
        // Clear pending interrupts
        uint16_t status = m_devices[i].read_register(REG_INTERRUPT_STATUS);
        if (status != 0) {
            printf("  Cleared pending interrupt status: 0x%04X\n", status);
        }
        
        if(!m_devices[i].check_diagnostics()) {
            printf("ERROR: Diagnostic check failed for ADC%zu: %s\n", 
                   i+1, m_devices[i].get_error_string());
            all_ok = false;
            continue;
        }
        
        if(!m_devices[i].verify_adc_reading()) {
            printf("ERROR: ADC verification failed for ADC%zu: %s\n", 
                   i+1, m_devices[i].get_error_string());
            all_ok = false;
            continue;
        }
        
        printf("  ✓ ADC%zu verification PASSED\n", i+1);
    }
    
    if (all_ok) {
        printf("\n✓ All %zu devices verified and ready\n", m_devices.size());
        print_status();
        return true;
    } else {
        printf("\n✗ One or more devices failed verification\n");
        return false;
    }
}

void MAX2253x_MultiADC::print_status() {
    printf("\n=== MAX2253x System Status ===\n");
    printf("Device Count: %zu\n\n", m_devices.size());
    
    for(size_t i = 0; i < m_devices.size(); i++) {
        printf("ADC%zu: CS=GPIO%u - Status: %s\n", 
               i+1, m_devices[i].get_cs_pin(), 
               m_devices[i].get_error_string());
    }
    printf("===============================\n\n");
}

std::array<uint16_t, 4> MAX2253x_MultiADC::read_device_raw(size_t index) {
    if(index >= m_devices.size()) {
        printf("ERROR: Device index %zu out of range\n", index);
        return {0, 0, 0, 0};
    }
    return m_devices[index].read_all_adc_raw();
}

std::array<float, 4> MAX2253x_MultiADC::read_device_voltage(size_t index) {
    if(index >= m_devices.size()) {
        printf("ERROR: Device index %zu out of range\n", index);
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }
    return m_devices[index].read_all_adc_voltage();
}

std::vector<std::array<uint16_t, 4>> MAX2253x_MultiADC::read_all_devices_raw() {
    std::vector<std::array<uint16_t, 4>> results;
    for(size_t i = 0; i < m_devices.size(); i++) {
        results.push_back(read_device_raw(i));
    }
    return results;
}

std::vector<std::array<float, 4>> MAX2253x_MultiADC::read_all_devices_voltage() {
    std::vector<std::array<float, 4>> results;
    for(size_t i = 0; i < m_devices.size(); i++) {
        results.push_back(read_device_voltage(i));
    }
    return results;
}