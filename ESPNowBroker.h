#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "UBLOXProcessor.h"
#include "espnow_protocol.h"
#include "helpers.h"

// === E S P N O W B R O K E R ===
//
// - Broadcasts GnssDelta packets to all ESP-NOW peers
// - No receive logic — GPS gateway does not accept commands
// - Owned by: UBLOXApplication

class ESPNowBroker {
public:
    explicit ESPNowBroker(UBLOXProcessor &processorRef);

    bool begin();
    void sendDelta();

private:
    UBLOXProcessor &_processor;
    bool _initialized = false;

    // Deadband state (same thresholds as SignalKBroker)
    float _last_lat  = NAN;
    float _last_lon  = NAN;
    float _last_sog  = NAN;
    float _last_cog  = NAN;

    static constexpr float DB_POS_DEG = 0.0001f;
    static constexpr float DB_SOG_MS  = 0.05f;
    static constexpr float DB_COG_RAD = 0.00873f;

    static constexpr uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    static void onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status);
};
