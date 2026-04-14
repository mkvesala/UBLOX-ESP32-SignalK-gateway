#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "UBLOXSensor.h"
#include "helpers.h"

// === U B L O X P R O C E S S O R ===
//
// - Owns unit conversion and navigation logic
// - begin() initialises sensor and probes for sensor-native magnetic variation
// - update() reads sensor, converts units, computes COG(M) when variation is known
// - setLiveMagVar() receives magnetic variation from SignalK subscription
// - getDelta() returns processed data for brokers
// - Owned by: UBLOXApplication
// - Uses: UBLOXSensor, TwoWire, GnssDelta (struct)

enum class MagVarSource : uint8_t {
    UNKNOWN  = 0,   // not yet determined
    SENSOR   = 1,   // module provides variation natively
    SIGNALK  = 2    // variation subscribed from SignalK server
};

class UBLOXProcessor {

public:

    explicit UBLOXProcessor(UBLOXSensor &sensorRef);

    bool begin(TwoWire &wirePort);
    bool update();

    // Delta for SignalK and ESP-NOW brokers
    struct GnssDelta {
        float lat_deg      = NAN;
        float lon_deg      = NAN;
        float sog_ms       = NAN;
        float cog_t_rad    = NAN;   // COG true (direct from u-blox)
        float cog_m_rad    = NAN;   // COG magnetic (computed, NAN if variation unknown)
        float mag_var_rad  = NAN;   // magnetic variation, NAN if unknown
        uint8_t satellites = 0;
        uint8_t fix_type   = 0;
        bool    fix_ok     = false;
    };

    GnssDelta getDelta() const { return _delta; }

    // Getters for display and web UI
    float    getLatDeg()     const { return _delta.lat_deg; }
    float    getLonDeg()     const { return _delta.lon_deg; }
    float    getSogMs()      const { return _delta.sog_ms; }
    float    getCogTRad()    const { return _delta.cog_t_rad; }
    float    getCogMRad()    const { return _delta.cog_m_rad; }
    float    getMagVarRad()  const { return _delta.mag_var_rad; }
    uint8_t  getSatellites() const { return _delta.satellites; }
    uint8_t  getFixType()    const { return _delta.fix_type; }
    bool     getFixOk()      const { return _delta.fix_ok; }

    // Magnetic variation source
    MagVarSource getMagVarSource() const { return _magvar_source; }
    bool needsMagVarSubscription() const { return _magvar_source == MagVarSource::SIGNALK; }

    // Called by SignalKBroker when magneticVariation arrives from server
    void setLiveMagVar(float rad);

private:

    UBLOXSensor &_sensor;

    GnssDelta _delta;
    MagVarSource _magvar_source = MagVarSource::UNKNOWN;
    float _magvar_rad = NAN;  // current best magnetic variation value

    // getMagDec() scale factor — verified at runtime and printed to Serial
    // UBX-NAV-PVT spec: I2, scale 1e-2 (degrees × 10^-2)
    static constexpr float MAG_DEC_SCALE = 100.0f;   // divide raw by this to get degrees

    // COG gating threshold — below this SOG, COG is noise
    static constexpr float COG_SOG_GATE_MS = 0.3f;   // ~0.6 kn

    // Probe sensor for native magnetic variation: take N readings and check if mag_dec != 0
    void probeSensorMagVar();

    void updateDelta(const RawGnssData &raw);

};
