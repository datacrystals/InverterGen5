// main.cpp
#include "pico/stdlib.h"
#include "PWMDriver.h"
#include "CommutationManager.h"
#include "Hardware.h"
#include "Command/CommandContext.h"
#include "Command/CommandManager.h"
#include "Command/SerialProcessor.h"
#include "Command/CommandInitializer.h"  // Single include for all command setup

static CommutationManager zone_mgr;
static PWMDriver* pwm_driver = nullptr;
static volatile float g_ramp_rate = 5.0f;
static volatile float g_manual_carrier_hz = COMMUTATION_PATTERN_DEFAULT_HZ;
static volatile bool g_manual_carrier_mode = false;
static float last_carrier_hz = 0.0f;
static bool last_sync_mode = false;
static uint16_t last_pulses = 0;

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
    zone_mgr.addAsyncFixed(0.0f, 15.0f, 2000.0f);
    zone_mgr.addSync(15.0f, 30.0f, 45);
    zone_mgr.addSync(30.0f, 45.0f, 31);
    zone_mgr.addSync(45.0f, 60.0f, 19);
}

int main() {
    stdio_init_all();
    sleep_ms(500);
    
    configureZones();
    
    PWMDriver::Config cfg;
    cfg.u_a = U_A; cfg.u_b = U_B;
    cfg.v_a = V_A; cfg.v_b = V_B;
    cfg.w_a = W_A; cfg.w_b = W_B;
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
    initializeCommands();  // Register all commands in one place
    
    SerialProcessor serial_proc;
    
    printf("\r\n3-Phase SPWM Controller\r\n");
    printf("Type 'HELP' or 'h' for commands\r\n");
    
    driver.enable();
    
    absolute_time_t last_print = get_absolute_time();
    
    while (true) {
        serial_proc.poll();
        
        if (!driver.isEmergencyStopped()) {
            driver.update(0.005f);
            updateCarrierFromZones();
        }
        
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
        
        sleep_ms(5);
    }
}