#pragma once

#include <cstddef>
#include <cstdint>

// Forward declare or include kvstore types if needed
// #include "kvstore.h"

class ConfigManager {
public:
    ConfigManager();
    
    // Initialize underlying flash storage. Call once from main() before other methods.
    bool init();
    
    // Scalar types (atomically stored as binary blobs)
    bool get(const char* key, bool& value);
    bool set(const char* key, bool value);
    
    bool get(const char* key, int32_t& value);
    bool set(const char* key, int32_t value);
    
    bool get(const char* key, float& value);
    bool set(const char* key, float value);
    
    // Small null-terminated strings (uses kvs_get_str)
    // bufferSize includes null terminator (\0)
    bool getString(const char* key, char* buffer, size_t bufferSize);
    bool setString(const char* key, const char* value);
    
    // Large binary/YAML blobs (100KB+)
    // Workflow: getSize() -> allocate buffer -> getLarge()
    bool getSize(const char* key, size_t& outSize);
    bool getLarge(const char* key, void* buffer, size_t bufferSize, size_t* outActualSize = nullptr);
    bool setLarge(const char* key, const void* data, size_t size);
    
    // Key management
    bool exists(const char* key);
    bool remove(const char* key);
    
    // Error diagnostics
    const char* getLastErrorString() const;
    int getLastErrorCode() const;
    void clearError();

private:
    const char* _lastErrStr;
    int _lastErrCode;
    
    void updateError(int code);
};