#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include "WifiState.h"
#include "UBLOXSensor.h"
#include "UBLOXProcessor.h"
#include "UBLOXPreferences.h"
#include "SignalKBroker.h"
#include "ESPNowBroker.h"
#include "DisplayManager.h"
#include "WebUIManager.h"

// === U B L O X A P P L I C A T I O N ===
//
// - Owns all subsystems as stack-allocated members
// - Wires dependencies via constructor initializer list
// - Manages WiFi state machine, timers, and loop handlers
// - Owned by: main .ino (global instance)

class UBLOXApplication {

public:

    explicit UBLOXApplication();

    void begin();
    void loop();
    bool sensorOk() const { return _sensor_ok; }

private:

    // Hardware
    static constexpr uint8_t I2C_SDA = 21;
    static constexpr uint8_t I2C_SCL = 22;

    // Feature flags
    static constexpr bool WEB_UI_ENABLED = true;

    // Timing constants
    static constexpr unsigned long READ_MS               = 167;    // ~6 Hz
    static constexpr unsigned long MIN_TX_INTERVAL_MS    = 199;    // ~5 Hz SignalK
    static constexpr unsigned long ESPNOW_TX_INTERVAL_MS = 211;    // ~5 Hz ESP-NOW
    static constexpr unsigned long WIFI_STATUS_CHECK_MS  = 491;
    static constexpr unsigned long WIFI_TIMEOUT_MS       = 179999; // ~3 mins
    static constexpr unsigned long WS_RETRY_MS           = 1999;
    static constexpr unsigned long WS_RETRY_MAX_MS       = 119993; // ~2 mins
    static constexpr unsigned long DIAG_INTERVAL_MS      = 30013;  // ~30 s

    // Timers
    unsigned long _expn_retry_ms      = WS_RETRY_MS;
    unsigned long _next_ws_try_ms     = 0;
    unsigned long _last_tx_ms         = 0;
    unsigned long _last_read_ms       = 0;
    unsigned long _wifi_conn_start_ms = 0;
    unsigned long _wifi_last_check_ms = 0;
    unsigned long _last_espnow_tx_ms  = 0;
    unsigned long _last_diag_ms       = 0;

    bool _sensor_ok = false;
    bool _wifi_services_started = false;
    WifiState _wifi_state = WifiState::INIT;

    // AP intruder detection — written in WiFi event callback, read in loop()
    volatile bool _ap_intruder = false;
    uint8_t _ap_intruder_mac[6] = {};

    // Subsystems — stack allocated, lifetime of the application
    UBLOXSensor _sensor;
    UBLOXProcessor _processor;
    UBLOXPreferences _prefs;
    SignalKBroker _signalk;
    ESPNowBroker _espnow;
    DisplayManager _display;
    WebUIManager _webui;

    // Loop handlers
    void handleWifi(unsigned long now);
    void handleOTA();
    void handleWebUI();
    void handleWebsocket(unsigned long now);
    void handleSensorRead(unsigned long now);
    void handleSignalK(unsigned long now);
    void handleESPNow(unsigned long now);
    void handleDisplay();
    void handleDiag(unsigned long now);
    void handleAPIntruder();

    void initWifiServices();
    void applyStaticIP();

};
