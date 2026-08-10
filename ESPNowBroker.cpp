#include "ESPNowBroker.h"

// === P U B L I C ===

ESPNowBroker::ESPNowBroker(UBLOXProcessor &processorRef)
    : _processor(processorRef)
{}

// Initialise ESP-NOW and register broadcast peer
bool ESPNowBroker::begin() {
    if (esp_now_init() != ESP_OK) {
        // Serial.println("[ESPNOW] init failed");
        return false;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        // Serial.println("[ESPNOW] add broadcast peer failed");
        return false;
    }

    esp_now_register_send_cb(onDataSent);

    _initialized = true;
    // Serial.println("[ESPNOW] begin() OK");
    return true;
}

// Broadcast GnssDelta packet — deadband gated
void ESPNowBroker::sendDelta() {
    if (!_initialized) return;

    auto d = _processor.getDelta();
    if (!d.fix_ok || !validf(d.lat_deg) || !validf(d.lon_deg)) return;

    // Deadband check
    uint32_t now = millis();
    bool pos_heartbeat = (now - _last_pos_tx_ms) >= POS_HEARTBEAT_MS;

    bool ch_pos = pos_heartbeat
                  || !validf(_last_lat) || !validf(_last_lon)
                  || fabsf(d.lat_deg - _last_lat) >= DB_POS_DEG
                  || fabsf(d.lon_deg - _last_lon) >= DB_POS_DEG;

    bool ch_sog = validf(d.sog_ms) &&
                  (!validf(_last_sog) || fabsf(d.sog_ms - _last_sog) >= DB_SOG_MS);

    bool ch_cog = validf(d.cog_t_rad) &&
                  (!validf(_last_cog) ||
                   fabsf(computeAngDiffRad(d.cog_t_rad, _last_cog)) >= DB_COG_RAD);

    if (!(ch_pos || ch_sog || ch_cog)) return;

    // Assemble ESPNowPacket<GnssDelta>
    ESPNow::ESPNowPacket<ESPNow::GnssDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::GNSS_DELTA, sizeof(ESPNow::GnssDelta));

    pkt.payload.lat_deg      = d.lat_deg;
    pkt.payload.lon_deg      = d.lon_deg;
    pkt.payload.sog_ms       = validf(d.sog_ms)      ? d.sog_ms      : NAN;
    pkt.payload.cog_true_rad = validf(d.cog_t_rad)   ? d.cog_t_rad   : NAN;
    pkt.payload.mag_var_rad  = validf(d.mag_var_rad) ? d.mag_var_rad : NAN;
    pkt.payload.satellites   = d.satellites;
    pkt.payload.fix_type     = d.fix_type;
    pkt.payload.fix_ok       = d.fix_ok ? 1 : 0;
    pkt.payload.reserved     = 0;

    esp_now_send(BROADCAST_ADDR, (const uint8_t*)&pkt, sizeof(pkt));

    if (ch_pos) { _last_lat = d.lat_deg; _last_lon = d.lon_deg; _last_pos_tx_ms = now; }
    if (ch_sog) { _last_sog = d.sog_ms; }
    if (ch_cog) { _last_cog = d.cog_t_rad; }
}

// Broadcast DateTimeDelta packet — GNSS UTC for receivers with no RTC and no NTP.
// Cadence is owned by UBLOXApplication::handleDateTime().
void ESPNowBroker::sendDateTime() {
    if (!_initialized) return;

    auto dt = _processor.getDateTime();
    if (!dt.valid) return;   // never broadcast a fabricated time

    // Assemble ESPNowPacket<DateTimeDelta>
    ESPNow::ESPNowPacket<ESPNow::DateTimeDelta> pkt;
    ESPNow::initHeader(pkt.hdr, ESPNow::ESPNowMsgType::DATETIME_DELTA, sizeof(ESPNow::DateTimeDelta));

    pkt.payload.unix_utc    = dt.unix_utc;
    pkt.payload.time_valid  = 1;
    pkt.payload.reserved[0] = 0;
    pkt.payload.reserved[1] = 0;
    pkt.payload.reserved[2] = 0;

    esp_now_send(BROADCAST_ADDR, (const uint8_t*)&pkt, sizeof(pkt));
}

// === S T A T I C ===

void ESPNowBroker::onDataSent(const esp_now_send_info_t* info, esp_now_send_status_t status) {
    (void)info;
    (void)status;
}
