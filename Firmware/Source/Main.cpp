// main.cpp
#include "pico/stdlib.h"
#include <Switching/PWMDriver.h>
#include <Switching/CommutationManager.h>
#include "Hardware.h"
#include "Command/CommandContext.h"
#include "Command/CommandManager.h"
#include "Command/SerialProcessor.h"
#include "Command/CommandInitializer.h"
#include "Sensors/MAX2253x.h"
#include "Sensors/MeasurementSystem.h"

static CommutationManager zone_mgr;
static PWMDriver* pwm_driver = nullptr;
static volatile float g_ramp_rate = 5.0f; //jerk hz/s
static volatile float g_manual_carrier_hz = Hardware::Commutation::DEFAULT_HZ;
static volatile bool g_manual_carrier_mode = false;
static float last_carrier_hz = 0.0f;
static bool last_sync_mode = false;
static uint16_t last_pulses = 0;

// Measurement system instances
static MAX2253x_MultiADC* adc_system = nullptr;
static MeasurementSystem* measurements = nullptr;

static void updateCarrierFromZones() {
    if (!pwm_driver || !pwm_driver->isEnabled()) return;
    
    if (g_manual_carrier_mode) {
        if (last_carrier_hz != g_manual_carrier_hz || last_sync_mode != false) {
            pwm_driver->setCarrierFrequency(g_manual_carrier_hz);
            pwm_driver->setSynchronousMode(false, 0);
            last_carrier_hz = g_manual_carrier_hz;
            last_sync_mode = false;
        }
    } else {
        float current_freq = pwm_driver->getCurrentFrequency();
        ZoneConfig zone;
        float sync_pulses = 0;
        
        if (zone_mgr.getZone(current_freq, &zone)) {
            float carrier = zone_mgr.calculateCarrier(current_freq, &zone, &sync_pulses);
            bool sync_mode = (zone.type == ZoneType::SYNC);
            uint16_t pulses = sync_mode ? static_cast<uint16_t>(sync_pulses) : 0;
            
            if (last_carrier_hz != carrier || last_sync_mode != sync_mode || last_pulses != pulses) {
                pwm_driver->setCarrierFrequency(carrier);
                pwm_driver->setSynchronousMode(sync_mode, pulses);
                last_carrier_hz = carrier;
                last_sync_mode = sync_mode;
                last_pulses = pulses;
            }
        }
    }
}

static void configureZones() {
    zone_mgr.clearZones();

    // -- alstom wmata 2000/3000/6000 switching pattern
    zone_mgr.addAsyncFixed(0.0f, 8.0f, 1235.0f);
    zone_mgr.addAsyncFixed(8.0f, 17.0f, 1190.0f);
    zone_mgr.addAsyncFixed(17.0f, 20.0f, 1210.0f);
    zone_mgr.addAsyncFixed(20.0f, 25.0f, 1235.0f);
    zone_mgr.addAsyncFixed(25.0f, 30.0f, 1460.0f);
    zone_mgr.addAsyncFixed(30.0f, 33.0f, 1210.0f);
    zone_mgr.addAsyncFixed(33.0f, 50.0f, 1230.0f);
    zone_mgr.addAsyncFixed(50.0f, 1000.0f, 1190.0f);
}

int main() {
    stdio_init_all();
    sleep_ms(500);
    
    configureZones();
    
    PWMDriver::Config cfg;
    cfg.min_duty_percent = 1.0f;
    cfg.max_duty_percent = 99.0f;
    
    static PWMDriver driver(cfg);
    pwm_driver = &driver;
    
    SPWMStrategy spwm;
    driver.setStrategy(&spwm);
    driver.setAutoModulation(true);
    driver.init(2000.0f);
    
    // Setup context
    CommandContext ctx = {
        .driver = pwm_driver,
        .zone_mgr = &zone_mgr,
        .ramp_rate = const_cast<float*>(&g_ramp_rate),
        .manual_carrier_hz = const_cast<float*>(&g_manual_carrier_hz),
        .manual_carrier_mode = const_cast<bool*>(&g_manual_carrier_mode),
        .last_carrier_hz = &last_carrier_hz,
        .last_sync_mode = &last_sync_mode,
        .last_pulses = &last_pulses,
        .update_carrier = updateCarrierFromZones
    };
    
    CommandManager::instance().setContext(ctx);
    initializeCommands();
    
    SerialProcessor serial_proc;
    
    printf("\r\n3-Phase SPWM Controller\r\n");
    printf("Type 'HELP' or 'h' for commands\r\n");
    
    // --- ADC and Measurement System Setup ---
    std::vector<uint8_t> cs_pins = {13, 14, 15, 22}; // 3 devices = 12 channels total
    
    static MAX2253x_MultiADC adc_instance(cs_pins);
    adc_system = &adc_instance;
    
    if (!adc_system->init()) {
        printf("FATAL: ADC initialization failed!\n");
        return -1;
    }
    
    static MeasurementSystem ms_instance(*adc_system);
    measurements = &ms_instance;
    
    // Configure your sensor layout here:
    // Scale factors example:
    // - Voltage divider: 200V max with 1:111 divider -> 1.8V ADC, scale = 111.11
    // - Bipolar current: Isolated amp with 0.9V=0A, 1.8V=+500A -> scale = 555.56 A/V, offset = 0.9V
    const std::vector<ChannelConfig> channel_map = {
        // Device 0 (CS=13): High Voltage and Auxiliary
        {0, 0, SensorType::VOLTAGE_DIVIDER, 1500.0f, 0.0f, 0.1f, "V_PH_W", 0.0f},  // Phase W
        {0, 1, SensorType::VOLTAGE_DIVIDER, 1500.0f, 0.0f, 0.1f, "V_PH_V", 0.0f},  // Phase V
        {0, 2, SensorType::VOLTAGE_DIVIDER, 1500.0f, 0.0f, 0.1f, "V_PH_U", 0.0f},  // Phase U
        {0, 3, SensorType::VOLTAGE_DIVIDER, 1500.0f, 0.0f, 1.0f, "V_DC_BUS", 0.0f} // DC Link bus
        
        // Device 1 (CS=14): Phase Currents (isolated shunt amplifiers)
        // Typical isolated current amp: 0.9V = 0A, 1.8V = +Imax, 0V = -Imax
        // For 500A peak: (1.8-0.9)V = 500A -> 555.56 A/V
        // {1, 0, SensorType::BIPOLAR_CURRENT, 555.56f, 0.0f, 0.5f, "I_PHASE_A", 0.9f},     // Phase A current
        // {1, 1, SensorType::BIPOLAR_CURRENT, 555.56f, 0.0f, 0.5f, "I_PHASE_B", 0.9f},     // Phase B current  
        // {1, 2, SensorType::BIPOLAR_CURRENT, 555.56f, 0.0f, 0.5f, "I_PHASE_C", 0.9f},     // Phase C current
        // {1, 3, SensorType::BIPOLAR_CURRENT, 111.11f, 0.0f, 0.5f, "I_DC_LINK", 0.9f},     // DC link current (lower range)
        
        // Device 2 (CS=15): Controls and Temperatures
        // {2, 0, SensorType::THROTTLE, 0.5556f, 0.0f, 0.2f, "THROTTLE", 0.0f},             // 0-1.8V -> 0-100%
        // {2, 1, SensorType::TEMPERATURE, 100.0f, -50.0f, 0.1f, "TEMP_IGBT", 0.0f},        // IGBT temp (e.g., LM35: 10mV/°C)
        // {2, 2, SensorType::TEMPERATURE, 100.0f, -50.0f, 0.1f, "TEMP_MOTOR", 0.0f},       // Motor temp
        // {2, 3, SensorType::DIRECT, 1.0f, 0.0f, 1.0f, "BRAKE_PEDAL", 0.0f}                // Brake sensor (raw)
    };
    
    measurements->addChannels(channel_map);
    measurements->printChannels();
    
    // Calibrate current sensors (ensure no current flowing!)
    printf("\nCalibrating current offsets...\n");
    sleep_ms(100);  // Let filters settle
    measurements->calibrateCurrentSensors();
    printf("Calibration complete.\n\n");
    
    driver.enable();
    
    absolute_time_t last_print = get_absolute_time();
    absolute_time_t last_telemetry = get_absolute_time();
    uint32_t loop_counter = 0;

    while (true) {
        // Update all measurements (reads all ADCs and applies scaling/filters)
        measurements->update();
        
        // Periodic telemetry output (every 500ms)
        if (absolute_time_diff_us(last_telemetry, get_absolute_time()) > 500000) {
            float v_dc = measurements->read("V_DC_BUS");
            float v_u = measurements->read("V_PH_U");
            float v_v = measurements->read("V_PH_V");
            float v_w = measurements->read("V_PH_W");
            
            printf("\r\n=== Telemetry ===\r\n");
            printf("DC Bus: %6.1fV | V_U: %5.1f%V | V_V: %5.1fV | V_W: %5.1fV\r\n", 
                   v_dc, v_u, v_v, v_w);
            
            // // Fault checking example
            // if (measurements->isChannelFaulted("V_DC_BUS")) {
            //     printf("WARNING: DC Bus voltage sensor fault!\r\n");
            // }
            // if (measurements->isChannelFaulted("I_PHASE_A")) {
            //     printf("WARNING: Phase A current sensor fault!\r\n");
            // }
            
            last_telemetry = get_absolute_time();
        }
        
        serial_proc.poll();
        
        if (!driver.isEmergencyStopped()) {
            driver.update(0.001f);
            updateCarrierFromZones();
        }
        
        // Existing status print (every 500ms)
        if (absolute_time_diff_us(last_print, get_absolute_time()) > 500000) {
            if (driver.isEmergencyStopped()) {
                printf("EMERGENCY STOP ACTIVE\r\n");
            } else if (!driver.isEnabled()) {
                printf("IDLE: Gates LOW, freq=0\r\n");
            } else {
                float current_freq = driver.getCurrentFrequency();
                float mod_index = driver.getModulationIndex();
                float carrier = driver.getCarrierFrequency();
                
                if (g_manual_carrier_mode) {
                    printf("MANUAL CARRIER F:%6.2fHz Mod:%3.0f%% Car:%4.0fHz [AUTO OFF]\r\n", 
                           current_freq, mod_index * 100.0f, carrier);
                } else {
                    ZoneConfig zone;
                    const char* zone_str = "DEF";
                    if (zone_mgr.getZone(current_freq, &zone)) {
                        switch(zone.type) {
                            case ZoneType::ASYNC_FIXED: zone_str = "ASYNC-FIX"; break;
                            case ZoneType::ASYNC_RAMP:  zone_str = "ASYNC-RMP"; break;
                            case ZoneType::SYNC:        zone_str = "SYNC"; break;
                        }
                    }
                    bool is_sync = driver.isSynchronousMode();
                    uint16_t pulses = driver.getPulsesPerCycle();
                    
                    printf("%s-%s F:%6.2fHz Mod:%3.0f%% n:%2d Car:%4.0fHz\r\n", 
                           zone_str, is_sync ? "SYNC" : "ASYNC",
                           current_freq, mod_index * 100.0f, pulses, carrier);
                }
            }
            last_print = get_absolute_time();
        }
        
        loop_counter++;
        sleep_ms(1);
    }
}