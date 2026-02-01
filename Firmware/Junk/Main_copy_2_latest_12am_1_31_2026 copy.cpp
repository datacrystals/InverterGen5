// main.cpp - Modular Zone-Based Carrier Management using PWMDriver & CommutationManager
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <stdio.h>

#include "PWMDriver.h"
#include "CommutationManager.h"
#include "Hardware.h"



// ==========================================
// GLOBALS
// ==========================================

static CommutationManager zone_mgr;
static PWMDriver* pwm_driver = nullptr;
static volatile float g_ramp_rate = 5.0f;

// Manual carrier override variables
static volatile float g_manual_carrier_hz = COMMUTATION_PATTERN_DEFAULT_HZ;
static volatile bool g_manual_carrier_mode = false;

// Track previous state to avoid unnecessary hardware updates
static float last_carrier_hz = 0.0f;
static bool last_sync_mode = false;
static uint16_t last_pulses = 0;

// ==========================================
// ZONE CONFIGURATION
// ==========================================

static void configureZones() {
    zone_mgr.clearZones();
    zone_mgr.addAsyncFixed(0.0f, 15.0f, 2000.0f);      // Zone 1: 0-15 Hz, fixed 2 kHz
    zone_mgr.addSync(15.0f, 30.0f, 45);                // Zone 2: 15-30 Hz, 45 pulses/cycle
    zone_mgr.addSync(30.0f, 45.0f, 31);                // Zone 3: 30-45 Hz, 31 pulses/cycle  
    zone_mgr.addSync(45.0f, 60.0f, 19);                // Zone 4: 45-60 Hz, 19 pulses/cycle
    // Add more zones as needed!
}

// ==========================================
// CARRIER UPDATE LOGIC
// ==========================================

static void updateCarrierFromZones() {
    if (!pwm_driver || !pwm_driver->isEnabled()) return;
    
    if (g_manual_carrier_mode) {
        // Manual override mode
        if (last_carrier_hz != g_manual_carrier_hz || last_sync_mode != false) {
            pwm_driver->setCarrierFrequency(g_manual_carrier_hz);
            pwm_driver->setSynchronousMode(false, 0);
            last_carrier_hz = g_manual_carrier_hz;
            last_sync_mode = false;
        }
    } else {
        // Automatic zone mode
        float current_freq = pwm_driver->getCurrentFrequency();
        ZoneConfig zone;
        float sync_pulses = 0;
        
        if (zone_mgr.getZone(current_freq, &zone)) {
            float carrier = zone_mgr.calculateCarrier(current_freq, &zone, &sync_pulses);
            bool sync_mode = (zone.type == ZoneType::SYNC);
            uint16_t pulses = sync_mode ? static_cast<uint16_t>(sync_pulses) : 0;
            
            // Only update hardware if something changed
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

// ==========================================
// MAIN
// ==========================================

int main() {
    stdio_init_all();
    sleep_ms(500);
    
    // Configure commutation zones
    configureZones();
    
    // Setup PWM driver hardware configuration
    PWMDriver::Config cfg;
    cfg.u_a = U_A; cfg.u_b = U_B;
    cfg.v_a = V_A; cfg.v_b = V_B;
    cfg.w_a = W_A; cfg.w_b = W_B;
    cfg.min_duty_percent = 1.0f;   // 1% minimum for bootstrap
    cfg.max_duty_percent = 99.0f;  // 99% maximum
    
    // Create driver instance (singleton for ISR)
    PWMDriver driver(cfg);
    pwm_driver = &driver;
    
    // Setup modulation strategy
    SPWMStrategy spwm;
    driver.setStrategy(&spwm);
    driver.setAutoModulation(true);  // Enable frequency-based modulation curve
    
    // Initialize but don't enable outputs yet
    driver.init(2000.0f);
    
    printf("\r\n3-Phase SPWM with CommutationManager + PWMDriver\r\n");
    printf("Commands: F<hz> R<hz/s> C<hz> A S s e I\r\n");
    printf("  F<hz>  - Set output frequency\r\n");
    printf("  R<hz/s> - Set ramp rate\r\n");
    printf("  C<hz>  - Set manual carrier frequency (100-10000 Hz)\r\n");
    printf("  A      - Return to automatic zone-based carrier\r\n");
    printf("  S      - Soft stop (ramp to 0)\r\n");
    printf("  s      - Emergency stop\r\n");
    printf("  e      - Enable after emergency stop\r\n");
    printf("  I<hz>  - Immediate frequency (no ramp)\r\n");

    // Enable PWM outputs
    driver.enable();

    char line[32];
    uint8_t line_idx = 0;
    absolute_time_t last_print = get_absolute_time();

    while (true) {
        // Serial input handling
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\r' || c == '\n') {
                if (line_idx > 0) {
                    line[line_idx] = '\0';
                    
                    if ((line[0] == 'F' || line[0] == 'f') && line_idx > 1) {
                        if (!driver.isEmergencyStopped()) {
                            float f = atof(line + 1);
                            if (f >= MIN_FUNDAMENTAL_FREQUENCY_HZ && f <= MAX_FUNDAMENTAL_FREQUENCY_HZ) {
                                driver.setTargetFrequency(f, g_ramp_rate);
                                if (f != 0.0f && !driver.isEnabled()) driver.enable();
                                printf("Target freq: %.2f Hz\r\n", f);
                            } else printf("Error: freq range -500..500\r\n");
                        } else printf("Error: Emergency stop active, press 'e' to enable\r\n");
                    }
                    else if ((line[0] == 'R' || line[0] == 'r') && line_idx > 1) {
                        float r = atof(line + 1);
                        if (r > 0 && r <= MAX_FUNDAMENTAL_FREQUENCY_RAMP_HZS) { 
                            g_ramp_rate = r; 
                            printf("Ramp rate: %.2f Hz/s\r\n", r); 
                        }
                    }
                    // Manual carrier frequency command
                    else if ((line[0] == 'C' || line[0] == 'c') && line_idx > 1) {
                        float carrier = atof(line + 1);
                        if (carrier >= MIN_SWITCHING_FREQUENCY_HZ && carrier <= MAX_SWITCHING_FREQUENCY_HZ) {
                            g_manual_carrier_hz = carrier;
                            g_manual_carrier_mode = true;
                            updateCarrierFromZones(); // Apply immediately
                            printf("Manual carrier: %.1f Hz (AUTO mode OFF)\r\n", carrier);
                        } else {
                            printf("Error: out of carrier range!\r\n");
                        }
                    }
                    // Return to automatic mode
                    else if (line[0] == 'A' || line[0] == 'a') {
                        g_manual_carrier_mode = false;
                        updateCarrierFromZones(); // Re-apply zone-based carrier
                        printf("AUTO mode: Zone-based carrier active\r\n");
                    }
                    else if (line[0] == 'S' && line_idx == 1) {
                        if (!driver.isEmergencyStopped()) { 
                            driver.setTargetFrequency(0.0f, g_ramp_rate); 
                            printf("Soft stop (ramp to 0)\r\n"); 
                        }
                    }
                    else if (line[0] == 's' && line_idx == 1) {
                        driver.emergencyStop();
                        printf("EMERGENCY STOP\r\n");
                    }
                    else if (line[0] == 'e' || line[0] == 'E') {
                        if (driver.isEmergencyStopped()) {
                            driver.clearEmergency();
                            printf("Re-enabled\r\n");
                        } else printf("Already enabled\r\n");
                    }
                    else if ((line[0] == 'I' || line[0] == 'i') && line_idx > 1) {
                        if (!driver.isEmergencyStopped()) {
                            float f = atof(line + 1);
                            if (f >= -200.0f && f <= 200.0f) {
                                driver.setFrequencyImmediate(f);
                                if (f != 0.0f && !driver.isEnabled()) driver.enable();
                                printf("Immediate: %.2f Hz\r\n", f);
                            }
                        } else printf("Error: Emergency stop active\r\n");
                    }
                    else {
                        printf("Unknown. Use: F<hz> R<hz/s> C<hz> A S s e I\r\n");
                    }
                    line_idx = 0;
                }
            } else if (line_idx < sizeof(line) - 1) {
                line[line_idx++] = (char)c;
            }
        }

        // Main loop logic
        if (!driver.isEmergencyStopped()) {
            // Update driver: handles frequency ramping and auto-modulation
            driver.update(0.005f);
            
            // Update carrier frequency based on zones
            updateCarrierFromZones();
        }

        // Status print (500ms interval)
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
                           zone_str,
                           is_sync ? "SYNC" : "ASYNC",
                           current_freq, mod_index * 100.0f, 
                           pulses, carrier);
                }
            }
            last_print = get_absolute_time();
        }

        sleep_ms(5);
    }
}