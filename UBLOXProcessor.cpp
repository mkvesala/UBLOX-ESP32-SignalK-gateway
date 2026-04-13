#include "UBLOXProcessor.h"

// === P U B L I C ===

UBLOXProcessor::UBLOXProcessor(UBLOXSensor &sensorRef)
    : _sensor(sensorRef)
{}

// Initialise sensor and determine eranto source
bool UBLOXProcessor::begin(TwoWire &wirePort) {
    if (!_sensor.begin(wirePort)) return false;

    // Give the module a moment to produce its first fix
    delay(500);

    // Probe for sensor-native magnetic eranto
    this->probeSensorEranto();

    return true;
}

// Read sensor and update processed values — call from handleSensorRead()
bool UBLOXProcessor::update() {
    RawGnssData raw;
    if (!_sensor.read(raw)) return false;

    this->updateDelta(raw);
    return true;
}

// Receive eranto (magnetic variation) from SignalK server subscription
void UBLOXProcessor::setLiveEranto(float rad) {
    if (!validf(rad)) return;
    _eranto_rad = rad;
    // If we previously had no source, mark it now
    if (_magvar_source == MagVarSource::UNKNOWN)
        _magvar_source = MagVarSource::SIGNALK;
}

// === P R I V A T E ===

// Probe up to 5 PVT frames to check if the module provides valid eranto.
// UBX-NAV-PVT magDec: I2, scale 1e-2 degrees.  mag_acc < 500 ≈ < 5.0°.
void UBLOXProcessor::probeSensorEranto() {
    Serial.println("[GNSS] Probing sensor for native magnetic eranto...");

    for (int i = 0; i < 5; i++) {
        delay(200);
        int16_t  dec = _sensor.getMagDec();
        uint16_t acc = _sensor.getMagAcc();

        Serial.printf("[GNSS] probe %d: mag_dec=%d mag_acc=%u\n", i, dec, acc);

        if (dec != 0 && acc < 500) {
            float eranto_deg = (float)dec / MAG_DEC_SCALE;
            _eranto_rad      = eranto_deg * DEG_TO_RAD;
            _magvar_source   = MagVarSource::SENSOR;
            Serial.printf("[GNSS] Sensor eranto OK: %.2f° (%.4f rad)\n",
                          eranto_deg, _eranto_rad);
            return;
        }
    }

    // Sensor does not provide eranto — will subscribe from SignalK
    _magvar_source = MagVarSource::SIGNALK;
    Serial.println("[GNSS] Sensor eranto not available — will subscribe from SignalK");
}

// Convert raw UBX-NAV-PVT fields to SI units and populate _delta
void UBLOXProcessor::updateDelta(const RawGnssData &raw) {
    _delta.satellites = raw.siv;
    _delta.fix_type   = raw.fix_type;
    _delta.fix_ok     = raw.fix_ok;

    if (!raw.fix_ok || raw.fix_type < 2) {
        // No usable fix — keep satellite and fix info but invalidate nav data
        _delta.lat_deg     = NAN;
        _delta.lon_deg     = NAN;
        _delta.sog_ms      = NAN;
        _delta.cog_t_rad   = NAN;
        _delta.cog_m_rad   = NAN;
        _delta.mag_var_rad = NAN;
        return;
    }

    // Position
    _delta.lat_deg = (float)raw.lat_e7 / 10000000.0f;
    _delta.lon_deg = (float)raw.lon_e7 / 10000000.0f;

    // Speed over ground: mm/s → m/s
    _delta.sog_ms = (float)raw.speed_mms / 1000.0f;

    // COG(T): degrees × 10^-5 → radians (true north referenced)
    // Only valid when moving fast enough
    if (_delta.sog_ms >= COG_SOG_GATE_MS) {
        _delta.cog_t_rad = normaliseRad((float)raw.heading_e5 / 100000.0f * DEG_TO_RAD);
    } else {
        _delta.cog_t_rad = NAN;
    }

    // Refresh eranto from sensor if that is the source
    if (_magvar_source == MagVarSource::SENSOR && raw.mag_acc < 500 && raw.mag_dec != 0) {
        _eranto_rad = ((float)raw.mag_dec / MAG_DEC_SCALE) * DEG_TO_RAD;
    }

    _delta.mag_var_rad = _eranto_rad;

    // COG(M) = COG(T) − eranto  (wrap to [0, 2π))
    if (validf(_delta.cog_t_rad) && validf(_eranto_rad)) {
        _delta.cog_m_rad = normaliseRad(_delta.cog_t_rad - _eranto_rad);
    } else {
        _delta.cog_m_rad = NAN;
    }
}
