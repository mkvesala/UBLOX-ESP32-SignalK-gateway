#include "UBLOXPreferences.h"

// === P U B L I C ===

UBLOXPreferences::UBLOXPreferences(UBLOXProcessor &processorRef)
    : _processor(processorRef)
{}

// Load all settings from NVS and push to processor
void UBLOXPreferences::load() {
    if (!_prefs.begin(_NS, false)) return;
    // Nothing to load yet — extend as settings are added
    _prefs.end();
}

// Save web password SHA256 hex string to NVS
void UBLOXPreferences::saveWebPassword(const char* password_sha256_hex) {
    if (!_prefs.begin(_NS, false)) return;
    _prefs.putString("web_pass", password_sha256_hex);
    _prefs.end();
}

// Load web password hash from NVS into caller-provided 65-byte buffer
bool UBLOXPreferences::loadWebPasswordHash(char* out_hash_64bytes) {
    if (!_prefs.begin(_NS, true)) {
        out_hash_64bytes[0] = '\0';
        return false;
    }
    size_t len = _prefs.getString("web_pass", out_hash_64bytes, 65);
    _prefs.end();
    if (len == 0 || strlen(out_hash_64bytes) != 64) {
        out_hash_64bytes[0] = '\0';
        return false;
    }
    return true;
}
