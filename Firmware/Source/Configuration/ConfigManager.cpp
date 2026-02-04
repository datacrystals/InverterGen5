#include "ConfigManager.h"
#include "kvstore.h"
#include <cstring>

ConfigManager::ConfigManager() 
    : _lastErrStr("OK"), _lastErrCode(0) {}

bool ConfigManager::init() {
    // Initialize pico-kvstore (mounts flash partition)
    kvs_init();
    clearError();
    return true;
}

void ConfigManager::updateError(int code) {
    _lastErrCode = code;
    _lastErrStr = (code == KVSTORE_SUCCESS) ? "OK" : kvs_strerror(code);
}

const char* ConfigManager::getLastErrorString() const {
    return _lastErrStr;
}

int ConfigManager::getLastErrorCode() const {
    return _lastErrCode;
}

void ConfigManager::clearError() {
    _lastErrCode = KVSTORE_SUCCESS;
    _lastErrStr = "OK";
}

// --- Boolean ---
bool ConfigManager::get(const char* key, bool& value) {
    uint8_t temp = 0;
    size_t actual = 0;
    int rc = kvs_get(key, &temp, sizeof(temp), &actual);
    if (rc == KVSTORE_SUCCESS && actual == sizeof(temp)) {
        value = (temp != 0);
        updateError(0);
        return true;
    }
    updateError(rc);
    return false;
}

bool ConfigManager::set(const char* key, bool value) {
    uint8_t temp = value ? 1 : 0;
    int rc = kvs_set(key, &temp, sizeof(temp));
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

// --- Integer ---
bool ConfigManager::get(const char* key, int32_t& value) {
    int32_t temp = 0;
    size_t actual = 0;
    int rc = kvs_get(key, &temp, sizeof(temp), &actual);
    if (rc == KVSTORE_SUCCESS && actual == sizeof(temp)) {
        value = temp;
        updateError(0);
        return true;
    }
    updateError(rc);
    return false;
}

bool ConfigManager::set(const char* key, int32_t value) {
    int rc = kvs_set(key, &value, sizeof(value));
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

// --- Float (stored as raw IEEE-754 bytes) ---
bool ConfigManager::get(const char* key, float& value) {
    uint32_t temp = 0;
    size_t actual = 0;
    int rc = kvs_get(key, &temp, sizeof(temp), &actual);
    if (rc == KVSTORE_SUCCESS && actual == sizeof(temp)) {
        std::memcpy(&value, &temp, sizeof(value));
        updateError(0);
        return true;
    }
    updateError(rc);
    return false;
}

bool ConfigManager::set(const char* key, float value) {
    uint32_t temp = 0;
    std::memcpy(&temp, &value, sizeof(value));
    int rc = kvs_set(key, &temp, sizeof(temp));
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

// --- Strings ---
bool ConfigManager::getString(const char* key, char* buffer, size_t bufferSize) {
    // kvs_get_str guarantees null termination
    int rc = kvs_get_str(key, buffer, bufferSize);
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

bool ConfigManager::setString(const char* key, const char* value) {
    // Store including null terminator so getString works correctly
    int rc = kvs_set(key, value, std::strlen(value) + 1);
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

// --- Large Binary / YAML ---
bool ConfigManager::getSize(const char* key, size_t& outSize) {
    outSize = 0;
    // Standard pattern: null buffer returns size without copying
    int rc = kvs_get(key, nullptr, 0, &outSize);
    // Note: If your kvstore version doesn't support nullptr buffer, 
    // replace with kvs_get_info() if available
    if (rc == KVSTORE_SUCCESS) {
        return true;
    }
    updateError(rc);
    return false;
}

bool ConfigManager::getLarge(const char* key, void* buffer, size_t bufferSize, size_t* outActualSize) {
    size_t dummy = 0;
    size_t* actual = outActualSize ? outActualSize : &dummy;
    int rc = kvs_get(key, buffer, bufferSize, actual);
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

bool ConfigManager::setLarge(const char* key, const void* data, size_t size) {
    int rc = kvs_set(key, data, size);
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}

// --- Management ---
bool ConfigManager::exists(const char* key) {
    size_t size = 0;
    int rc = kvs_get(key, nullptr, 0, &size);
    return (rc == KVSTORE_SUCCESS);
}

bool ConfigManager::remove(const char* key) {
    int rc = kvs_remove(key);  // Note: If your API uses kvs_delete(), rename here
    updateError(rc);
    return rc == KVSTORE_SUCCESS;
}