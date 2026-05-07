#include "UBLOXApplication.h"
#include "secrets.h"

// === P U B L I C ===

// Constructor — wire all dependencies via initializer list
UBLOXApplication::UBLOXApplication()
    : _sensor()
    , _processor(_sensor)
    , _prefs(_processor)
    , _signalk(_processor)
    , _espnow(_processor)
    , _display(_processor, _signalk)
    , _webui(_processor, _prefs, _signalk, _display)
{}

// Initialise hardware and start subsystems
void UBLOXApplication::begin() {
    // 1. I2C bus — GNSS (0x42) and LCD (0x27/0x3F) share the same bus
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(47);
    Wire.setClock(400000);
    delay(47);

    // 2. Display first — shows init messages
    _display.begin();

    // 3. Sensor and processor
    _sensor_ok = _processor.begin(Wire);

    // 4. Load NVS settings
    _prefs.load();

    // 5. Bluetooth not needed
    btStop();

    // Log reset reason — reveals watchdog, panic, stack overflow etc.
    esp_reset_reason_t rr = esp_reset_reason();
    // Serial.printf("[APP] Reset reason: %d\n", (int)rr);

    // 6. WiFi AP_STA required for ESP-NOW + WiFi coexistence.
    //    softAP() secures the AP interface immediately: hidden SSID, WPA2 password,
    //    max 1 connection. This does not affect ESP-NOW (operates below association layer).
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS, 1 /*channel*/, 1 /*ssid_hidden*/, 1 /*max_connection*/);

    // Register AP intruder handler — fires in FreeRTOS "arduino_events" task.
    // Deauth the intruder immediately, then signal loop() via volatile flag.
    // MAC is copied before setting the flag so loop() always reads a complete address.
    WiFi.onEvent([this](arduino_event_id_t /*id*/, arduino_event_info_t info) {
        uint8_t aid = info.wifi_ap_staconnected.aid;
        memcpy(_ap_intruder_mac, info.wifi_ap_staconnected.mac, 6);
        esp_wifi_deauth_sta(aid);  // kick immediately — ESP-IDF call
        _ap_intruder = true;       // signal loop()
    }, ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    _wifi_state = WifiState::CONNECTING;
    _wifi_conn_start_ms = millis();

    // Serial.printf("[APP] WiFi connecting to %s...\n", WIFI_SSID);

    // 7. ESP-NOW works independently of WiFi connection
    _espnow.begin();
}

// Main loop — dispatch all handlers
void UBLOXApplication::loop() {
    const unsigned long now = millis();
    this->handleWifi(now);
    this->handleAPIntruder();
    this->handleOTA();
    this->handleWebUI();
    this->handleWebsocket(now);
    this->handleSensorRead(now);
    this->handleSignalK(now);
    this->handleESPNow(now);
    this->handleDisplay();
    this->handleDiag(now);
}

// === P R I V A T E ===

// WiFi state machine — runs every WIFI_STATUS_CHECK_MS
void UBLOXApplication::handleWifi(unsigned long now) {
    if ((long)(now - _wifi_last_check_ms) < (long)WIFI_STATUS_CHECK_MS) return;
    _wifi_last_check_ms = now;

    switch (_wifi_state) {
        case WifiState::INIT:
            break;

        case WifiState::CONNECTING: {
            wl_status_t status = WiFi.status();
            if (status == WL_CONNECTED) {
                _wifi_state = WifiState::CONNECTED;
                // Serial.printf("[WIFI] Connected — IP %s\n", WiFi.localIP().toString().c_str());
                _display.showInfoMessage("WiFi OK", WiFi.localIP().toString().c_str());
                this->initWifiServices();
                _expn_retry_ms = WS_RETRY_MS;
            } else if ((long)(now - _wifi_conn_start_ms) >= (long)WIFI_TIMEOUT_MS) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                // Serial.println("[WIFI] Connection timeout — WiFi off");
                _display.showInfoMessage("WiFi timeout", "Running offline");
            } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
                _wifi_state = WifiState::OFF;
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                // Serial.println("[WIFI] Connection failed — WiFi off");
                _display.showInfoMessage("WiFi failed", "Running offline");
            }
            break;
        }

        case WifiState::CONNECTED:
            if (!WiFi.isConnected()) {
                _wifi_state = WifiState::CONNECTING;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                _wifi_conn_start_ms = now;
                // Serial.println("[WIFI] Lost — reconnecting...");
            }
            break;

        case WifiState::FAILED:
        case WifiState::DISCONNECTED:
        case WifiState::OFF:
            break;
    }
}

// Initialise WiFi-dependent services — called once only (guard prevents repeat on reconnect)
void UBLOXApplication::initWifiServices() {
    if (_wifi_services_started) return;
    _wifi_services_started = true;

    _signalk.begin();

    ArduinoOTA.setHostname(_signalk.getSignalKSource());
    ArduinoOTA.setPassword(OTA_PASS);
    ArduinoOTA.begin();

    if (WEB_UI_ENABLED) _webui.begin();
}

// OTA update handler — requires WiFi
void UBLOXApplication::handleOTA() {
    if (_wifi_state != WifiState::CONNECTED) return;
    ArduinoOTA.handle();
}

// Web UI handler — requires WiFi
void UBLOXApplication::handleWebUI() {
    if (!WEB_UI_ENABLED) return;
    if (_wifi_state != WifiState::CONNECTED) return;
    _webui.handleRequest();
}

// WebSocket connection management with exponential backoff
void UBLOXApplication::handleWebsocket(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    _signalk.handleStatus();

    if (!_signalk.isOpen() && (long)(now - _next_ws_try_ms) >= 0) {
        _signalk.connectWebsocket();
        _next_ws_try_ms = now + _expn_retry_ms;
        _expn_retry_ms  = min(_expn_retry_ms * 2, WS_RETRY_MAX_MS);
    }
    if (_signalk.isOpen()) _expn_retry_ms = WS_RETRY_MS;
}

// Read GNSS sensor at ~6 Hz
void UBLOXApplication::handleSensorRead(unsigned long now) {
    if ((long)(now - _last_read_ms) < (long)READ_MS) return;
    _last_read_ms = now;
    _processor.update();
}

// Send SignalK delta at ~5 Hz
void UBLOXApplication::handleSignalK(unsigned long now) {
    if (_wifi_state != WifiState::CONNECTED) return;
    if ((long)(now - _last_tx_ms) < (long)MIN_TX_INTERVAL_MS) return;
    _last_tx_ms = now;
    _signalk.sendDelta();
}

// Send ESP-NOW broadcast at ~5 Hz
void UBLOXApplication::handleESPNow(unsigned long now) {
    if ((long)(now - _last_espnow_tx_ms) < (long)ESPNOW_TX_INTERVAL_MS) return;
    _last_espnow_tx_ms = now;
    _espnow.sendDelta();
}

// Update display — runs every loop iteration (rate-limited inside DisplayManager)
void UBLOXApplication::handleDisplay() {
    _display.handle();
}

// AP intruder alert — deauth already done in event callback; log + display here
void UBLOXApplication::handleAPIntruder() {
    if (!_ap_intruder) return;
    _ap_intruder = false;  // clear before Serial/display so a rapid second event isn't lost
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             _ap_intruder_mac[0], _ap_intruder_mac[1], _ap_intruder_mac[2],
             _ap_intruder_mac[3], _ap_intruder_mac[4], _ap_intruder_mac[5]);
    // Serial.printf("[AP] INTRUDER deauthed — MAC %s\n", mac);
    _display.showInfoMessage("AP: INTRUDER!", mac);
}

// Periodic heap/stack diagnostics — every ~30 s
void UBLOXApplication::handleDiag(unsigned long now) {
    if ((long)(now - _last_diag_ms) < (long)DIAG_INTERVAL_MS) return;
    _last_diag_ms = now;

    uint32_t heap = ESP.getFreeHeap();
    uint32_t wm = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
    // Serial.printf("[DIAG] heap=%lu B  stack_wm=%lu B\n",
    //               (unsigned long)heap, (unsigned long)wm);

    // if (heap < 20000)
    //     Serial.printf("[DIAG] WARNING: low heap %lu B\n", (unsigned long)heap);

    float mv = _processor.getMagVarRad();
    // if (validf(mv))
    //     Serial.printf("[DIAG] magvar=%.2f° src=wmm\n", mv * RAD_TO_DEG);
    // else
    //     Serial.printf("[DIAG] magvar=N/A src=wmm\n");

    _display.showDiagData(heap, wm);
}
