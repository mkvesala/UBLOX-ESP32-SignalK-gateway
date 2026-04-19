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
// - Sends navigation delta (position, SOG, COG, magnetic variation)
// - Subscribes to navigation.magneticVariation when sensor variation unavailable
// - Owned by: UBLOXApplication
// - Owns: WebsocketsClient
// - Uses: UBLOXProcessor

namespace websockets {
    class WebsocketsClient;
    class WebsocketsMessage;
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

    // Reusable JSON documents (stack allocated)
    // position obj + 4 float paths — 640 bytes is sufficient
    StaticJsonDocument<640> _delta_doc;
    StaticJsonDocument<1024> _incoming_doc;
    StaticJsonDocument<256> _subscribe_doc;

    bool _ws_open = false;
    char _sk_url[512] {};
    char _sk_source[32] {};

    // Deadband thresholds
    static constexpr float    DB_POS_DEG        = 0.00001f;  // ~1.1 m
    static constexpr float    DB_SOG_MS         = 0.05f;     // ~0.1 kn
    static constexpr float    DB_COG_RAD        = 0.00873f;  // 0.5°
    static constexpr float    DB_VAR_RAD        = 0.00873f;  // 0.5°
    static constexpr uint32_t POS_HEARTBEAT_MS  = 10000;     // force position every 10 s

    uint32_t _last_pos_tx_ms = 0;

    void setSignalKURL();
    void setSignalKSource();
    void subscribeToMagneticVariation();

    void onMessageCallback(websockets::WebsocketsMessage msg);
    void onEventCallback(websockets::WebsocketsEvent event);
    
};
