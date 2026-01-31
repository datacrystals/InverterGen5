#ifndef COMMAND_TYPES_H
#define COMMAND_TYPES_H

#include <cstdint>
#include <stdio.h>

struct ArgValue {
    float f_val;      // For FLOAT type
    int32_t i_val;    // For INT type  
    bool present;     // Whether argument was provided
};

struct ArgSpec {
    const char* name;        // e.g., "freq", "rate"
    const char* unit;        // e.g., "Hz", "Hz/s"
    float min;               // Minimum value (inclusive)
    float max;               // Maximum value (inclusive)
    float default_val;       // Used if not required and not provided
    bool required;           // If true, must be present
    enum Type { FLOAT, INT } type;
    
    // Helper for formatting ranges in help
    void printRange(char* buf, size_t size) const {
        if (type == FLOAT) {
            snprintf(buf, size, "%.1f-%.1f %s", min, max, unit);
        } else {
            snprintf(buf, size, "%d-%d %s", (int)min, (int)max, unit);
        }
    }
};

#endif