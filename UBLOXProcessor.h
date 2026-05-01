#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WMM_Tinier.h>
#include "UBLOXSensor.h"
#include "helpers.h"

// === U B L O X P R O C E S S O R ===
//
// - Owns unit conversion and navigation logic
// - begin() initialises sensor and WMM model
// - update() reads sensor, converts units, computes magnetic variation via WMM
// - getDelta() returns processed data for brokers
// - Owned by: UBLOXApplication
// - Uses: UBLOXSensor, TwoWire, GnssDelta (struct)
// - Owns: WMM_Tinier

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
        float mag_var_rad  = NAN;   // magnetic variation from WMM, NAN until first fix
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
    float    getMagVarRad()  const { return _delta.mag_var_rad; }
    uint8_t  getSatellites() const { return _delta.satellites; }
    uint8_t  getFixType()    const { return _delta.fix_type; }
    bool     getFixOk()      const { return _delta.fix_ok; }

private:

    UBLOXSensor &_sensor;
    WMM_Tinier _wmm;

    GnssDelta _delta;
    float _magvar_rad = NAN;

    // COG gating threshold — below this SOG, COG is noise
    static constexpr float COG_SOG_GATE_MS = 0.2f;   // ~0.4 kn

    void updateDelta(const RawGnssData &raw);

};
