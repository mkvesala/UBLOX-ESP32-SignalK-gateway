#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <esp_mac.h>
#include "UBLOXProcessor.h"
#include "helpers.h"

// === S I G N A L K B R O K E R ===
//
// - WebSocket connection to SignalK server
// - Sends: navigation.position, speedOverGround, courseOverGroundTrue, magneticVariation,
//          navigation.gnss.satellites, navigation.gnss.type, navigation.gnss.methodQuality
// - Owned by: UBLOXApplication
// - Owns: WebsocketsClient
// - Uses: UBLOXProcessor

namespace websockets {
    class WebsocketsClient;
    enum class WebsocketsEvent;
}

class SignalKBroker {

public:

    explicit SignalKBroker(UBLOXProcessor &processorRef);

    bool begin();
    void handleStatus();
    bool connectWebsocket();
    void closeWebsocket();
    void sendDelta();

    bool isOpen() const { return _ws_open; }
    const char* getSignalKSource() const { return _sk_source; }

private:

    UBLOXProcessor &_processor;
    websockets::WebsocketsClient _ws;

    // Reusable JSON document — position obj + 5 value paths
    StaticJsonDocument<768> _delta_doc;

    bool _ws_open = false;
    char _sk_url[512] {};
    char _sk_source[32] {};

    // Deadband thresholds
    static constexpr float    DB_POS_DEG        = 0.000005f;  // ~0.5 m
    static constexpr float    DB_SOG_MS         = 0.025f;     // ~0.05 kn
    static constexpr float    DB_COG_RAD        = 0.001745f;  // 0.1°
    static constexpr float    DB_VAR_RAD        = 0.001745f;  // 0.1°
    static constexpr uint32_t POS_HEARTBEAT_MS  = 500;     // force position every 0.5 s

    uint32_t _last_pos_tx_ms = 0;

    void setSignalKURL();
    void setSignalKSource();
    void onEventCallback(websockets::WebsocketsEvent event);
    static const char* methodQualityStr(uint8_t ft);
    
};
