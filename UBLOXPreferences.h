#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "UBLOXProcessor.h"

// === U B L O X P R E F E R E N C E S ===
//
// - NVS-backed configuration storage
// - Skeleton: load() and web password helpers implemented
// - Future: manual variation override, etc.
// - Owned by: UBLOXApplication

class UBLOXPreferences {
public:
    explicit UBLOXPreferences(UBLOXProcessor &processorRef);

    void load();

    void saveWebPassword(const char* password_sha256_hex);
    bool loadWebPasswordHash(char* out_hash_64bytes);

private:
    static constexpr const char* _NS = "ublox";

    Preferences    _prefs;
    UBLOXProcessor &_processor;
};
