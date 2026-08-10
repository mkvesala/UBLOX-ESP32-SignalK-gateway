#include "UBLOXProcessor.h"

// === P U B L I C ===

UBLOXProcessor::UBLOXProcessor(UBLOXSensor &sensorRef)
    : _sensor(sensorRef)
{}

// Initialise sensor and WMM model
bool UBLOXProcessor::begin(TwoWire &wirePort) {
    if (!_sensor.begin(wirePort)) return false;
    _wmm.begin();
    return true;
}

// Read sensor and update processed values — call from handleSensorRead()
bool UBLOXProcessor::update() {
    RawGnssData raw;
    if (!_sensor.read(raw)) return false;

    this->updateDelta(raw);
    this->updateDateTime(raw);
    return true;
}

// === P R I V A T E ===

// Convert raw UBX-NAV-PVT fields to SI units and populate _delta
void UBLOXProcessor::updateDelta(const RawGnssData &raw) {
    _delta.satellites = raw.siv;
    _delta.fix_type = raw.fix_type;
    _delta.fix_ok = raw.fix_ok;

    if (!raw.fix_ok || raw.fix_type < 2) {
        _delta.lat_deg     = NAN;
        _delta.lon_deg     = NAN;
        _delta.sog_ms      = NAN;
        _delta.cog_t_rad   = NAN;
        _delta.mag_var_rad = NAN;
        return;
    }

    // Position
    _delta.lat_deg = (float)raw.lat_e7 / 10000000.0f;
    _delta.lon_deg = (float)raw.lon_e7 / 10000000.0f;

    // Speed over ground: mm/s → m/s
    _delta.sog_ms = (float)raw.speed_mms / 1000.0f;

    // COG(T): degrees × 10^-5 → radians (true north referenced)
    if (_delta.sog_ms >= COG_SOG_GATE_MS) {
        _delta.cog_t_rad = normaliseRad((float)raw.heading_e5 / 100000.0f * DEG_TO_RAD);
    } else {
        _delta.cog_t_rad = NAN;
    }

    // Magnetic variation from WMM — requires valid GPS date
    if (raw.year > 2020 && raw.month >= 1 && raw.month <= 12 && raw.day >= 1) {
        float dec_deg = _wmm.magneticDeclination(
            _delta.lat_deg, _delta.lon_deg,
            (uint8_t)(raw.year - 2000), raw.month, raw.day);
        _magvar_rad = dec_deg * DEG_TO_RAD;
    }

    _delta.mag_var_rad = _magvar_rad;
}

// Accept GNSS UTC only when the module confirms it and the epoch is plausible.
// Sticky like _magvar_rad — a momentary loss of confirmation must not blank a clock
// that receivers have already synchronised to.
void UBLOXProcessor::updateDateTime(const RawGnssData &raw) {
    if (!raw.time_valid || raw.unix_utc < MIN_PLAUSIBLE_EPOCH) return;

    _datetime.unix_utc = raw.unix_utc;
    _datetime.valid    = true;
}
