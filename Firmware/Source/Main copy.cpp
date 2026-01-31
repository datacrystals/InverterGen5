// main.cpp - Modular Zone-Based Carrier Management with Manual Override
// Configure zones with simple function calls - no hard-coded magic numbers!

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include <stdio.h>

// GPIO mapping
#define U_A 16
#define U_B 17
#define V_A 18
#define V_B 19
#define W_A 20
#define W_B 21



// ==========================================
// GLOBALS & HARDWARE
// ==========================================

// Easy configuration - EDIT THESE CALLS TO CHANGE BEHAVIOR
static ZoneManager zone_mgr;
static void configureZones() {
    zone_mgr.clearZones();
    zone_mgr.addAsyncFixed(0.0f, 15.0f, 2000.0f);      // Zone 1: 0-15 Hz, fixed 2 kHz
    zone_mgr.addSync(15.0f, 30.0f, 45); // Zone 2: 15-30 Hz, ramp up
    zone_mgr.addSync(30.0f, 45.0f, 31); // Zone 2: 15-30 Hz, ramp up
    zone_mgr.addSync(45.0f, 60.0f, 19); // Zone 2: 15-30 Hz, ramp up
    // zone_mgr.addAsyncRamp(15.0f, 30.0f, 2000.0f, 6000.0f); // Zone 2: 15-30 Hz, ramp up
    // zone_mgr.addAsyncFixed(30.0f, 200.0f, 6000.0f);    // Zone 3: 30+ Hz, fixed 6 kHz
    // Add more zones as needed!
}

// Global variables
static volatile float g_target_freq = 0.0f;
static volatile float g_current_freq = 0.0f;
static volatile float g_ramp_rate = 5.0f;
static volatile bool g_emergency_stop = false;
static volatile bool g_pwm_enabled = true;
static const uint32_t PWM_SLICE_MASK = (1u << 0) | (1u << 1) | (1u << 2);

static volatile float g_carrier_hz = 2000.0f;
static volatile float g_clkdiv = 1.0f;
static volatile uint16_t g_top = 0;
static volatile float g_mod_index = 0.05f;
static volatile float g_dtheta = 0.0f;
static volatile float theta = 0.0f;

static volatile bool g_synchronous_mode = false;
static volatile uint16_t g_pulses_per_cycle = 33;

// NEW: Manual carrier override variables
static volatile float g_manual_carrier_hz = 2000.0f;
static volatile bool g_manual_carrier_mode = false;

// [All your existing helper functions remain unchanged...]
static inline void compute_three_phase(float th, float *su, float *sv, float *sw) {
    *su = sinf(th);
    *sv = sinf(th - 2.0f * (float)M_PI / 3.0f);
    *sw = sinf(th + 2.0f * (float)M_PI / 3.0f);
}

static inline uint16_t clamp_u16(uint16_t x, uint16_t lo, uint16_t hi) {
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

static inline uint16_t duty_from_sine(float s) {
    float x = (g_mod_index * s + 1.0f) * 0.5f;
    if (x < 0.0f) x = 0.0f;
    if (x > 1.0f) x = 1.0f;
    uint16_t d = (uint16_t)(x * (float)g_top);
    uint16_t lo = (uint16_t)(g_top / 100);
    uint16_t hi = (uint16_t)(g_top - g_top / 100);
    return clamp_u16(d, lo, hi);
}

static inline void set_slice_complementary(uint slice, uint16_t level) {
    pwm_set_chan_level(slice, PWM_CHAN_A, level);
    pwm_set_chan_level(slice, PWM_CHAN_B, level);
}

static void force_all_gpio_low(void) {
    gpio_set_function(U_A, GPIO_FUNC_SIO); gpio_set_dir(U_A, GPIO_OUT); gpio_put(U_A, 0);
    gpio_set_function(U_B, GPIO_FUNC_SIO); gpio_set_dir(U_B, GPIO_OUT); gpio_put(U_B, 0);
    gpio_set_function(V_A, GPIO_FUNC_SIO); gpio_set_dir(V_A, GPIO_OUT); gpio_put(V_A, 0);
    gpio_set_function(V_B, GPIO_FUNC_SIO); gpio_set_dir(V_B, GPIO_OUT); gpio_put(V_B, 0);
    gpio_set_function(W_A, GPIO_FUNC_SIO); gpio_set_dir(W_A, GPIO_OUT); gpio_put(W_A, 0);
    gpio_set_function(W_B, GPIO_FUNC_SIO); gpio_set_dir(W_B, GPIO_OUT); gpio_put(W_B, 0);
}

static void restore_pwm_pins(void) {
    gpio_set_function(U_A, GPIO_FUNC_PWM); gpio_set_function(U_B, GPIO_FUNC_PWM);
    gpio_set_function(V_A, GPIO_FUNC_PWM); gpio_set_function(V_B, GPIO_FUNC_PWM);
    gpio_set_function(W_A, GPIO_FUNC_PWM); gpio_set_function(W_B, GPIO_FUNC_PWM);
}

static void disable_pwm_outputs(void) {
    g_pwm_enabled = false;
    pwm_set_mask_enabled(0); force_all_gpio_low();
    for (uint slice = 0; slice < 3; slice++) {
        pwm_set_chan_level(slice, PWM_CHAN_A, g_top / 2);
        pwm_set_chan_level(slice, PWM_CHAN_B, g_top / 2);
    }
}

static void enable_pwm_outputs(void) {
    restore_pwm_pins(); sleep_us(10);
    theta = 0.0f; pwm_set_counter(0, 0); pwm_set_counter(1, 0); pwm_set_counter(2, 0);
    pwm_set_mask_enabled(PWM_SLICE_MASK); g_pwm_enabled = true;
}

static void update_pwm_hardware(float carrier_hz) {
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    float ideal_top_f = (float)sys_hz / (2.0f * carrier_hz) - 1.0f;
    float div = 1.0f; uint16_t new_top;
    
    if (ideal_top_f > 65535.0f) {
        div = ceilf((ideal_top_f / 65535.0f) * 10.0f) / 10.0f;
        if (div > 255.0f) div = 255.0f;
        new_top = (uint16_t)((float)sys_hz / (2.0f * carrier_hz * div) - 1.0f);
    } else {
        new_top = (uint16_t)ideal_top_f;
    }
    
    irq_set_enabled(PWM_IRQ_WRAP, false);
    for (uint slice = 0; slice < 3; slice++) {
        pwm_set_clkdiv(slice, div);
        pwm_set_wrap(slice, new_top);
    }
    g_clkdiv = div; g_top = new_top; g_carrier_hz = carrier_hz;
    irq_set_enabled(PWM_IRQ_WRAP, true);
}

// MODIFIED: Carrier update with manual override support
static void update_carrier(void) {
    if (g_manual_carrier_mode) {
        // Manual override mode - use fixed carrier regardless of zones
        update_pwm_hardware(g_manual_carrier_hz);
        g_synchronous_mode = false; // Manual mode always asynchronous
        g_pulses_per_cycle = 0;
    } else {
        // Automatic zone mode
        ZoneConfig zone;
        float sync_pulses = 0;
        
        if (zone_mgr.getZone(g_current_freq, &zone)) {
            float carrier = zone_mgr.calculateCarrier(g_current_freq, &zone, &sync_pulses);
            update_pwm_hardware(carrier);
            
            // Update mode and pulses if in sync zone
            g_synchronous_mode = (zone.type == ZONE_SYNC);
            if (g_synchronous_mode) {
                g_pulses_per_cycle = (uint16_t)sync_pulses;
            }
        }
    }
}

static void update_phase_step(void) {
    if (g_synchronous_mode && fabsf(g_current_freq) > 0.01f) {
        g_dtheta = 2.0f * (float)M_PI / (float)g_pulses_per_cycle;
    } else {
        g_dtheta = 2.0f * (float)M_PI * g_current_freq / g_carrier_hz;
    }
}

extern "C" void pwm_wrap_isr(void);
extern "C" void pwm_wrap_isr(void) {
    pwm_clear_irq(0);
    if (g_emergency_stop || !g_pwm_enabled) return;

    float su, sv, sw;
    compute_three_phase(theta, &su, &sv, &sw);
    set_slice_complementary(0, duty_from_sine(su));
    set_slice_complementary(1, duty_from_sine(sv));
    set_slice_complementary(2, duty_from_sine(sw));

    theta += g_dtheta;
    if (theta >= 2.0f * (float)M_PI) theta -= 2.0f * (float)M_PI;
    if (theta < 0.0f) theta += 2.0f * (float)M_PI;
}

static void init_slice_pair(uint gpio_a, uint gpio_b, uint slice, pwm_config *cfg) {
    gpio_set_function(gpio_a, GPIO_FUNC_PWM);
    gpio_set_function(gpio_b, GPIO_FUNC_PWM);
    pwm_init(slice, cfg, false);
    pwm_set_output_polarity(slice, false, true);
    pwm_set_chan_level(slice, PWM_CHAN_A, g_top / 2);
    pwm_set_chan_level(slice, PWM_CHAN_B, g_top / 2);
}

int main() {

    stdio_init_all();
    sleep_ms(500);
    
    // Configure zones - EDIT HERE!
    configureZones();
    
    printf("\r\n3-Phase SPWM with ZONE MANAGEMENT + MANUAL CARRIER OVERRIDE\r\n");
    printf("Commands: F<hz> R<hz/s> C<hz> A S s e I\r\n");
    printf("  F<hz>  - Set output frequency\r\n");
    printf("  R<hz/s> - Set ramp rate\r\n");
    printf("  C<hz>  - Set manual carrier frequency (100-10000 Hz)\r\n");
    printf("  A      - Return to automatic zone-based carrier\r\n");
    printf("  S      - Soft stop (ramp to 0)\r\n");
    printf("  s      - Emergency stop\r\n");
    printf("  e      - Enable after emergency stop\r\n");
    printf("  I<hz>  - Immediate frequency (no ramp)\r\n");

    const uint32_t sys_hz = clock_get_hz(clk_sys);
    update_carrier();

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, g_clkdiv);
    pwm_config_set_wrap(&cfg, g_top);
    pwm_config_set_phase_correct(&cfg, false);

    init_slice_pair(U_A, U_B, 0, &cfg);
    init_slice_pair(V_A, V_B, 1, &cfg);
    init_slice_pair(W_A, W_B, 2, &cfg);

    // Start sequence
    pwm_set_counter(0, 0); pwm_set_counter(1, 0); pwm_set_counter(2, 0);
    pwm_set_mask_enabled(PWM_SLICE_MASK); sleep_us(50);
    pwm_set_mask_enabled(0);
    pwm_set_phase_correct(0, true); pwm_set_phase_correct(1, true); pwm_set_phase_correct(2, true);
    pwm_set_counter(0, 0); pwm_set_counter(1, 0); pwm_set_counter(2, 0);
    pwm_set_mask_enabled(PWM_SLICE_MASK);

    pwm_clear_irq(0); pwm_set_irq_enabled(0, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwm_wrap_isr);
    irq_set_enabled(PWM_IRQ_WRAP, true);

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
                        if (!g_emergency_stop) {
                            float f = atof(line + 1);
                            if (f >= -500.0f && f <= 500.0f) {
                                g_target_freq = f;
                                if (f != 0.0f && !g_pwm_enabled) enable_pwm_outputs();
                                printf("Target freq: %.2f Hz\r\n", f);
                            } else printf("Error: freq range -500..500\r\n");
                        } else printf("Error: Emergency stop active, press 'e' to enable\r\n");
                    }
                    else if ((line[0] == 'R' || line[0] == 'r') && line_idx > 1) {
                        float r = atof(line + 1);
                        if (r > 0 && r <= 500.0f) { g_ramp_rate = r; printf("Ramp rate: %.2f Hz/s\r\n", r); }
                    }
                    // NEW: Manual carrier frequency command
                    else if ((line[0] == 'C' || line[0] == 'c') && line_idx > 1) {
                        float carrier = atof(line + 1);
                        if (carrier >= 100.0f && carrier <= 10000.0f) {
                            g_manual_carrier_hz = carrier;
                            g_manual_carrier_mode = true;
                            update_carrier(); // Apply immediately
                            printf("Manual carrier: %.1f Hz (AUTO mode OFF)\r\n", carrier);
                        } else {
                            printf("Error: carrier range 100..10000 Hz\r\n");
                        }
                    }
                    // NEW: Return to automatic mode
                    else if (line[0] == 'A' || line[0] == 'a') {
                        g_manual_carrier_mode = false;
                        update_carrier(); // Re-apply zone-based carrier
                        printf("AUTO mode: Zone-based carrier active\r\n");
                    }
                    else if (line[0] == 'S' && line_idx == 1) {
                        if (!g_emergency_stop) { g_target_freq = 0.0f; printf("Soft stop (ramp to 0)\r\n"); }
                    }
                    else if (line[0] == 's' && line_idx == 1) {
                        g_emergency_stop = true; g_pwm_enabled = false;
                        pwm_set_mask_enabled(0); force_all_gpio_low();
                        g_target_freq = 0.0f; g_current_freq = 0.0f; theta = 0.0f;
                        printf("EMERGENCY STOP\r\n");
                    }
                    else if (line[0] == 'e' || line[0] == 'E') {
                        if (g_emergency_stop) {
                            g_emergency_stop = false; restore_pwm_pins();
                            sleep_us(10); enable_pwm_outputs();
                            printf("Re-enabled\r\n");
                        } else printf("Already enabled\r\n");
                    }
                    else if ((line[0] == 'I' || line[0] == 'i') && line_idx > 1) {
                        if (!g_emergency_stop) {
                            float f = atof(line + 1);
                            if (f >= -200.0f && f <= 200.0f) {
                                g_target_freq = f; g_current_freq = f;
                                if (f != 0.0f && !g_pwm_enabled) enable_pwm_outputs();
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
        if (!g_emergency_stop) {
            float err = g_target_freq - g_current_freq;
            if (fabsf(err) > 0.01f) {
                const float dt = 0.005f;
                float step = g_ramp_rate * dt;
                if (err > 0) g_current_freq += (err > step ? step : err);
                else         g_current_freq -= (-err > step ? step : -err);
                update_carrier(); // Update on every change (checks manual mode flag)
            }

            if (g_pwm_enabled && g_target_freq == 0.0f && fabsf(g_current_freq) < 0.01f) {
                disable_pwm_outputs();
                printf("PWM disabled: all gates forced LOW\r\n");
            }

            float mod; float abs_freq = fabsf(g_current_freq);
            if (abs_freq <= 0.0f) mod = 0.05f;
            else if (abs_freq >= 60.0f) mod = 0.99f;
            else mod = 0.05f + (abs_freq / 60.0f) * (0.99f - 0.05f);
            g_mod_index = mod;

            update_phase_step();
        }

        // Status print
        if (absolute_time_diff_us(last_print, get_absolute_time()) > 500000) {
            if (g_emergency_stop) {
                printf("EMERGENCY STOP ACTIVE\r\n");
            } else if (!g_pwm_enabled) {
                printf("IDLE: Gates LOW, freq=0\r\n");
            } else {
                // MODIFIED: Show manual mode status
                if (g_manual_carrier_mode) {
                    printf("MANUAL CARRIER F:%6.2fHz Mod:%3.0f%% Car:%4.0fHz [AUTO OFF]\r\n", 
                           g_current_freq, g_mod_index * 100.0f, g_carrier_hz);
                } else {
                    ZoneConfig zone;
                    const char* zone_str = "DEF";
                    if (zone_mgr.getZone(g_current_freq, &zone)) {
                        switch(zone.type) {
                            case ZONE_ASYNC_FIXED: zone_str = "ASYNC-FIX"; break;
                            case ZONE_ASYNC_RAMP:  zone_str = "ASYNC-RMP"; break;
                            case ZONE_SYNC:        zone_str = "SYNC"; break;
                        }
                    }
                    printf("%s-%s F:%6.2fHz Mod:%3.0f%% n:%2d Car:%4.0fHz\r\n", 
                           zone_str,
                           g_synchronous_mode ? "SYNC" : "ASYNC",
                           g_current_freq, g_mod_index * 100.0f, 
                           g_pulses_per_cycle, g_carrier_hz);
                }
            }
            last_print = get_absolute_time();
        }

        sleep_ms(5);
    }
}




// #include "PWMDriver.h"
// #include "CommutationManager.h"  // Your previous file

// // Pin definitions for your hardware
// #define U_A 0
// #define U_B 1
// #define V_A 2
// #define V_B 3
// #define W_A 4
// #define W_B 5

// int main() {
//     stdio_init_all();
    
//     // Setup PWM driver
//     PWMDriver::Config cfg;
//     cfg.u_a = U_A; cfg.u_b = U_B;
//     cfg.v_a = V_A; cfg.v_b = V_B;
//     cfg.w_a = W_A; cfg.w_b = W_B;
    
//     PWMDriver pwm(cfg);
//     SPWMStrategy spwm;
//     pwm.setStrategy(&spwm);
//     pwm.init(2000.0f);  // Start with 2kHz carrier
    
//     // Setup your zone manager
//     CommutationManager zones;
//     zones.addAsyncFixed(0.0f, 30.0f, 2000.0f);
//     zones.addAsyncRamp(30.0f, 60.0f, 2000.0f, 1000.0f);
//     zones.addSync(60.0f, 500.0f, 33);
    
//     // Main loop
//     pwm.setTargetFrequency(50.0f, 50.0f);  // Ramp to 50Hz at 50Hz/s
    
//     while (true) {
//         // Update driver (handles ramping)
//         pwm.update(0.005f);  // 5ms loop time
        
//         // Update carrier from zones
//         ZoneConfig zone;
//         float sync_pulses;
//         if (zones.getZone(pwm.getCurrentFrequency(), &zone)) {
//             float carrier = zones.calculateCarrier(pwm.getCurrentFrequency(), &zone, &sync_pulses);
//             pwm.setCarrierFrequency(carrier);
            
//             // Handle sync mode
//             pwm.setSynchronousMode(zone.type == ZoneType::SYNC, 
//                                   static_cast<uint16_t>(sync_pulses));
//         }
        
//         sleep_ms(5);
//     }
// }