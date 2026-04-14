#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "UBLOXProcessor.h"

// === U B L O X P R E F E R E N C E S ===
//
// - NVS-backed configuration storage
// - Skeleton
// - Future: manual variation override, etc.
// - Owned by: UBLOXApplication
// - Owns: Preferences
// - Uses: UBLOXProcessor

class UBLOXPreferences {

public:

    explicit UBLOXPreferences(UBLOXProcessor &processorRef);

    void load() {};
    void save() {};

private:

    static constexpr const char* _NS = "ublox";

    Preferences _prefs;
    UBLOXProcessor &_processor;

};
