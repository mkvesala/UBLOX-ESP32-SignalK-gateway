#include "SignalKBroker.h"
#include "secrets.h"

using namespace websockets;

// === P U B L I C ===

SignalKBroker::SignalKBroker(UBLOXProcessor &processorRef)
    : _processor(processorRef)
{}

// Build URL and source name, open websocket
bool SignalKBroker::begin() {
    if (strlen(SK_HOST) <= 0 || SK_PORT <= 0) return false;
    this->setSignalKURL();
    this->setSignalKSource();
    return this->connectWebsocket();
}

// Poll websocket to keep connection alive — call from loop()
void SignalKBroker::handleStatus() {
    if (_ws_open) _ws.poll();
}

// Connect to SignalK server and register callbacks
bool SignalKBroker::connectWebsocket() {
    _ws_open = _ws.connect(_sk_url);
    if (_ws_open) {
        _ws.onMessage([this](WebsocketsMessage msg) {
            this->onMessageCallback(msg);
        });
        _ws.onEvent([this](WebsocketsEvent event, const String &data) {
            this->onEventCallback(event);
        });
    }
    return _ws_open;
}

// Close websocket (used from restart handler)
void SignalKBroker::closeWebsocket() {
    _ws.close();
    _ws_open = false;
}

// Send GNSS navigation delta to SignalK server
void SignalKBroker::sendDelta() {
    if (!_ws_open) return;

    auto d = _processor.getDelta();
    if (!d.fix_ok || !validf(d.lat_deg) || !validf(d.lon_deg)) return;

    // --- Deadband tracking ---
    static float last_lat = NAN, last_lon = NAN;
    static float last_sog = NAN;
    static float last_cog_t = NAN, last_cog_m = NAN;
    static float last_var = NAN;

    bool ch_pos = (!validf(last_lat) || !validf(last_lon)
                   || fabsf(d.lat_deg - last_lat) >= DB_POS_DEG
                   || fabsf(d.lon_deg - last_lon) >= DB_POS_DEG);

    bool ch_sog = validf(d.sog_ms) &&
                  (!validf(last_sog) || fabsf(d.sog_ms - last_sog) >= DB_SOG_MS);

    bool ch_cog_t = validf(d.cog_t_rad) &&
                    (!validf(last_cog_t) ||
                     fabsf(computeAngDiffRad(d.cog_t_rad, last_cog_t)) >= DB_COG_RAD);

    bool ch_cog_m = validf(d.cog_m_rad) &&
                    (!validf(last_cog_m) ||
                     fabsf(computeAngDiffRad(d.cog_m_rad, last_cog_m)) >= DB_COG_RAD);

    bool ch_var = validf(d.mag_var_rad) &&
                  (!validf(last_var) || fabsf(d.mag_var_rad - last_var) >= DB_VAR_RAD);

    if (!(ch_pos || ch_sog || ch_cog_t || ch_cog_m || ch_var)) return;

    // --- Build JSON delta ---
    _delta_doc.clear();
    _delta_doc["context"] = "vessels.self";
    auto updates = _delta_doc.createNestedArray("updates");
    auto up      = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values  = up.createNestedArray("values");

    // Helper for simple float paths
    auto add = [&](const char* path, float v) {
        auto o    = values.createNestedObject();
        o["path"] = path;
        o["value"] = v;
    };

    // navigation.position — nested object
    if (ch_pos) {
        auto o    = values.createNestedObject();
        o["path"] = "navigation.position";
        auto pos  = o.createNestedObject("value");
        pos["latitude"]  = d.lat_deg;
        pos["longitude"] = d.lon_deg;
        last_lat = d.lat_deg;
        last_lon = d.lon_deg;
    }

    if (ch_sog)   { add("navigation.speedOverGround",          d.sog_ms);      last_sog   = d.sog_ms; }
    if (ch_cog_t) { add("navigation.courseOverGroundTrue",     d.cog_t_rad);   last_cog_t = d.cog_t_rad; }
    if (ch_cog_m) { add("navigation.courseOverGroundMagnetic", d.cog_m_rad);   last_cog_m = d.cog_m_rad; }
    if (ch_var)   { add("navigation.magneticVariation",        d.mag_var_rad); last_var   = d.mag_var_rad; }

    if (values.size() == 0) return;

    char buf[640];
    size_t n = serializeJson(_delta_doc, buf, sizeof(buf));
    bool ok = _ws.send(buf, n);
    if (!ok) {
        _ws.close();
        _ws_open = false;
    }
}

// === P R I V A T E ===

// Build SignalK websocket URL from secrets.h constants
void SignalKBroker::setSignalKURL() {
    if (strlen(SK_TOKEN) > 0)
        snprintf(_sk_url, sizeof(_sk_url),
                 "ws://%s:%d/signalk/v1/stream?token=%s", SK_HOST, SK_PORT, SK_TOKEN);
    else
        snprintf(_sk_url, sizeof(_sk_url),
                 "ws://%s:%d/signalk/v1/stream", SK_HOST, SK_PORT);
}

// Build unique source name from last 3 bytes of ESP32 MAC address
void SignalKBroker::setSignalKSource() {
    uint8_t m[6];
    esp_efuse_mac_get_default(m);
    snprintf(_sk_source, sizeof(_sk_source), "esp32.ublox-%02x%02x%02x", m[3], m[4], m[5]);
}

// Subscribe to navigation.magneticVariation at ~1 Hz (CMPS14-pattern)
void SignalKBroker::subscribeToMagneticVariation() {
    _subscribe_doc.clear();
    _subscribe_doc["context"] = "vessels.self";
    auto subscribe = _subscribe_doc.createNestedArray("subscribe");
    auto s         = subscribe.createNestedObject();
    s["path"]   = "navigation.magneticVariation";
    s["format"] = "delta";
    s["policy"] = "ideal";
    s["period"] = 1000;

    char buf[256];
    size_t n = serializeJson(_subscribe_doc, buf, sizeof(buf));
    _ws.send(buf, n);
    Serial.println("[SK] Subscribed to navigation.magneticVariation");
}

// Handle incoming SignalK delta — extract eranto when subscribed
void SignalKBroker::onMessageCallback(WebsocketsMessage msg) {
    if (!_processor.needsErantoSubscription()) return;
    if (!msg.isText()) return;
    _incoming_doc.clear();
    if (deserializeJson(_incoming_doc, msg.data())) return;
    if (!_incoming_doc.containsKey("updates")) return;

    for (JsonObject up : _incoming_doc["updates"].as<JsonArray>()) {
        if (!up.containsKey("values")) continue;
        for (JsonObject v : up["values"].as<JsonArray>()) {
            if (!v.containsKey("path")) continue;
            const char* path = v["path"];
            if (!path) continue;
            if (strcmp(path, "navigation.magneticVariation") == 0) {
                if (v["value"].is<float>() || v["value"].is<double>()) {
                    float mv = v["value"].as<float>();
                    if (validf(mv)) {
                        _processor.setLiveEranto(mv);
                    }
                }
            }
        }
    }
}

// Handle websocket events
void SignalKBroker::onEventCallback(WebsocketsEvent event) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            _ws_open = true;
            // Subscribe to eranto from server if sensor cannot provide it
            if (_processor.needsErantoSubscription())
                this->subscribeToMagneticVariation();
            break;
        case WebsocketsEvent::ConnectionClosed:
            _ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            _ws.pong();
            break;
        case WebsocketsEvent::GotPong:
        default:
            break;
    }
}
