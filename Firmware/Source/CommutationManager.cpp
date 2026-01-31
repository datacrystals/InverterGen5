#include "CommutationManager.h"

CommutationManager::CommutationManager() : zone_count(0) {
    // Zones array left uninitialized until clearZones() or add*() called
}

void CommutationManager::clearZones() { 
    zone_count = 0; 
}

void CommutationManager::addAsyncFixed(float start_freq, float end_freq, float carrier_hz) {
    if (zone_count >= MAX_ZONES) return;
    zones[zone_count] = {start_freq, end_freq, ZoneType::ASYNC_FIXED, carrier_hz, 0.0f};
    zone_count++;
}

void CommutationManager::addAsyncRamp(float start_freq, float end_freq, 
                                     float carrier_start, float carrier_end) {
    if (zone_count >= MAX_ZONES) return;
    zones[zone_count] = {start_freq, end_freq, ZoneType::ASYNC_RAMP, carrier_start, carrier_end};
    zone_count++;
}

void CommutationManager::addSync(float start_freq, float end_freq, uint16_t pulses_per_cycle) {
    if (zone_count >= MAX_ZONES) return;
    zones[zone_count] = {start_freq, end_freq, ZoneType::SYNC, 
                        static_cast<float>(pulses_per_cycle), 0.0f};
    zone_count++;
}

bool CommutationManager::getZone(float freq, ZoneConfig* zone) const {
    float abs_freq = std::fabs(freq);
    for (int i = 0; i < zone_count; i++) {
        if (abs_freq >= zones[i].freq_start && abs_freq < zones[i].freq_end) {
            *zone = zones[i];
            return true;
        }
    }
    // Safe default: async fixed at default carrier if no zone matches
    *zone = {0.0f, 1000.0f, ZoneType::ASYNC_FIXED, DEFAULT_CARRIER, 0.0f};
    return false;
}

float CommutationManager::calculateCarrier(float freq, const ZoneConfig* zone, 
                                          float* sync_pulses) const {
    float abs_freq = std::fabs(freq);
    
    switch (zone->type) {
        case ZoneType::ASYNC_FIXED:
            return zone->param1;
        
        case ZoneType::ASYNC_RAMP: {
            float range = zone->freq_end - zone->freq_start;
            if (range <= 0.0f) return zone->param1;
            float ratio = (abs_freq - zone->freq_start) / range;
            return zone->param1 - ratio * (zone->param1 - zone->param2);
        }
        
        case ZoneType::SYNC: {
            if (sync_pulses) *sync_pulses = zone->param1;
            float carrier = abs_freq * zone->param1;
            // Clamp to hardware limits (protect CM600HA-24H IGBT module)
            if (carrier < MIN_CARRIER) carrier = MIN_CARRIER;
            if (carrier > MAX_CARRIER) carrier = MAX_CARRIER;
            return carrier;
        }
    }
    return DEFAULT_CARRIER;
}

float CommutationManager::getCarrierForFrequency(float freq, float* sync_pulses) const {
    ZoneConfig zone;
    if (getZone(freq, &zone)) {
        return calculateCarrier(freq, &zone, sync_pulses);
    }
    return DEFAULT_CARRIER;
}