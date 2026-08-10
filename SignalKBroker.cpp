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
    if (_ws_open && _ws) _ws->poll();
}

// Connect to SignalK server and register callbacks
bool SignalKBroker::connectWebsocket() {
    // Fresh client per attempt — a stuck lwIP socket is never inherited
    _ws = std::make_unique<WebsocketsClient>();
    _ws->onEvent([this](WebsocketsEvent event, const String &data) {
        (void)data;
        this->onEventCallback(event);
    });
    _ws_open = _ws->connect(_sk_url);
    if (_ws_open) {
        _last_pong_ms = millis();   // seed liveness so a fresh socket is not flagged stale
    } else {
        _ws.reset();                // destroy failed client → free the socket fd
    }
    return _ws_open;
}

// Close websocket (used from restart handler)
void SignalKBroker::closeWebsocket() {
    if (_ws) {
        _ws->close();
        _ws.reset();               // destroy client → WiFiClient dtor frees lwIP socket fd
    }
    _ws_open = false;
    _last_pong_ms = 0;
}

// Send a client-initiated ping frame to probe liveness
void SignalKBroker::ping() {
    if (_ws_open && _ws) _ws->ping();
}

// Half-open detection: open, has been connected, but no pong within timeout
bool SignalKBroker::isStale(unsigned long now) const {
    return _ws_open && _last_pong_ms != 0 &&
           (long)(now - _last_pong_ms) >= (long)PONG_TIMEOUT_MS;
}

// Send GNSS navigation delta to SignalK server
void SignalKBroker::sendDelta() {
    if (!_ws_open) return;

    auto d = _processor.getDelta();
    if (!d.fix_ok || !validf(d.lat_deg) || !validf(d.lon_deg)) return;

    // --- Deadband tracking ---
    static float   last_lat      = NAN;
    static float   last_lon      = NAN;
    static float   last_sog      = NAN;
    static float   last_cog_t    = NAN;
    static float   last_var      = NAN;
    static uint8_t last_sat      = 0xFF;  // 0xFF = not yet sent
    static uint8_t last_fix_type = 0xFF;

    uint32_t now = millis();
    bool pos_heartbeat = (now - _last_pos_tx_ms) >= POS_HEARTBEAT_MS;

    bool ch_pos = pos_heartbeat
                  || !validf(last_lat) || !validf(last_lon)
                  || fabsf(d.lat_deg - last_lat) >= DB_POS_DEG
                  || fabsf(d.lon_deg - last_lon) >= DB_POS_DEG;

    bool ch_sog = validf(d.sog_ms) &&
                  (!validf(last_sog) || fabsf(d.sog_ms - last_sog) >= DB_SOG_MS);

    bool ch_cog_t = validf(d.cog_t_rad) &&
                    (!validf(last_cog_t) ||
                     fabsf(computeAngDiffRad(d.cog_t_rad, last_cog_t)) >= DB_COG_RAD);

    bool ch_var = validf(d.mag_var_rad) &&
                  (pos_heartbeat || !validf(last_var) || fabsf(d.mag_var_rad - last_var) >= DB_VAR_RAD);

    bool ch_sat = (last_sat == 0xFF) || (d.satellites != last_sat) || (d.fix_type != last_fix_type);

    if (!(ch_pos || ch_sog || ch_cog_t || ch_var || ch_sat)) return;

    // --- Build JSON delta ---
    _delta_doc.clear();
    _delta_doc["context"] = "vessels.self";
    auto updates = _delta_doc.createNestedArray("updates");
    auto up = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values = up.createNestedArray("values");

    // Helper for simple float paths
    auto add = [&](const char* path, float v) {
        auto o = values.createNestedObject();
        o["path"] = path;
        o["value"] = v;
    };

    // navigation.position — nested object
    if (ch_pos) {
        auto o = values.createNestedObject();
        o["path"] = "navigation.position";
        auto pos = o.createNestedObject("value");
        pos["latitude"] = d.lat_deg;
        pos["longitude"] = d.lon_deg;
        last_lat = d.lat_deg;
        last_lon = d.lon_deg;
        _last_pos_tx_ms = now;
    }

    if (ch_sog)   { add("navigation.speedOverGround",      d.sog_ms);      last_sog   = d.sog_ms; }
    if (ch_cog_t) { add("navigation.courseOverGroundTrue", d.cog_t_rad);   last_cog_t = d.cog_t_rad; }
    if (ch_var)   { add("navigation.magneticVariation",    d.mag_var_rad); last_var   = d.mag_var_rad; }

    // Always include GNSS status fields with every transmission
    { auto o = values.createNestedObject(); o["path"] = "navigation.gnss.satellites";     o["value"] = (int)d.satellites; }
    { auto o = values.createNestedObject(); o["path"] = "navigation.gnss.type";           o["value"] = "Combined GPS+GLONASS"; }
    { auto o = values.createNestedObject(); o["path"] = "navigation.gnss.methodQuality";  o["value"] = methodQualityStr(d.fix_type); }
    last_sat      = d.satellites;
    last_fix_type = d.fix_type;

    char buf[768];
    size_t n = serializeJson(_delta_doc, buf, sizeof(buf));
    bool ok = _ws->send(buf, n);
    if (!ok) {
        this->closeWebsocket();   // destroy the client; backoff loop reconnects fresh
    }
}

// Send GNSS UTC to SignalK as navigation.datetime.
// Format comes from the SignalK schema (schemas/groups/navigation.json): a string in
// RFC 3339, UTC only without local offset. The schema pattern is ".*Z$", so the
// trailing Z is mandatory and "+00:00" would be rejected.
// gnssTimeSource is a sibling property of value in the full model, not a leaf, so it
// cannot be expressed as a delta path — navigation.gnss.type already carries it.
void SignalKBroker::sendDateTime() {
    if (!_ws_open) return;

    auto dt = _processor.getDateTime();
    if (!dt.valid) return;

    time_t    t = (time_t)dt.unix_utc;
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);

    // char[] rather than const char* — ArduinoJson copies arrays into the document
    // but only aliases a const char*, which would dangle once this returns
    char iso[21];
    strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);

    // --- Build JSON delta ---
    _dt_doc.clear();
    _dt_doc["context"] = "vessels.self";
    auto updates = _dt_doc.createNestedArray("updates");
    auto up = updates.createNestedObject();
    up["$source"] = _sk_source;
    auto values = up.createNestedArray("values");
    auto o = values.createNestedObject();
    o["path"]  = "navigation.datetime";
    o["value"] = iso;

    char buf[256];
    size_t n = serializeJson(_dt_doc, buf, sizeof(buf));
    bool ok = _ws->send(buf, n);
    if (!ok) {
        this->closeWebsocket();   // destroy the client; backoff loop reconnects fresh
    }
}

// === P R I V A T E ===

// Map UBX fix_type (0–4) to SignalK navigation.gnss.methodQuality enum value
const char* SignalKBroker::methodQualityStr(uint8_t ft) {
    switch (ft) {
        case 1:  return "Estimated (DR) mode";
        case 2:  return "GNSS Fix";
        case 3:  return "GNSS Fix";
        case 4:  return "GNSS Fix";
        default: return "no GPS";
    }
}

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

// Handle websocket events
void SignalKBroker::onEventCallback(WebsocketsEvent event) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            _ws_open = true;
            _last_pong_ms = millis();
            break;
        case WebsocketsEvent::ConnectionClosed:
            _ws_open = false;
            break;
        case WebsocketsEvent::GotPing:
            if (_ws) _ws->pong();
            break;
        case WebsocketsEvent::GotPong:
            _last_pong_ms = millis();
            break;
        default:
            break;
    }
}
