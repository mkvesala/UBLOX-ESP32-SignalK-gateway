#include "UBLOXSensor.h"

// === P U B L I C ===

UBLOXSensor::UBLOXSensor() {}

// Initialise I2C connection and configure the MAX-M10S
bool UBLOXSensor::begin(TwoWire &wirePort) {
    _wire = &wirePort;

    if (!_gnss.begin(wirePort, GNSS_ADDR)) {
        // Serial.println("[GNSS] begin() failed — module not found at 0x42");
        return false;
    }

    // Suppress NMEA output on I2C, keep UBX only
    _gnss.setI2COutput(COM_TYPE_UBX);

    // Set navigation rate to 6 Hz (measurement period 167 ms)
    _gnss.setNavigationFrequency(6);

    // Enable automatic PVT output so getPVT(0) works non-blocking
    _gnss.setAutoPVT(true);

    // Serial.println("[GNSS] begin() OK — 6 Hz, UBX only, autoPVT on");
    return true;
}

// Check if module responds on I2C
bool UBLOXSensor::available() const {
    if (!_wire) return false;
    _wire->beginTransmission(GNSS_ADDR);
    return (_wire->endTransmission() == 0);
}

// Non-blocking PVT read.
// getPVT(0): maxWait=0 returns immediately if no fresh data is ready.
// NOTE: if this proves unreliable, increase to getPVT(250) — acceptable
//       within the 167 ms handleSensorRead interval.
bool UBLOXSensor::read(RawGnssData &out) {
    if (!_gnss.getPVT(0)) return false;

    out.lat_e7      = _gnss.getLatitude();
    out.lon_e7      = _gnss.getLongitude();
    out.speed_mms   = _gnss.getGroundSpeed();
    out.heading_e5  = _gnss.getHeading();
    out.year        = _gnss.getYear();
    out.month       = _gnss.getMonth();
    out.day         = _gnss.getDay();
    out.siv         = _gnss.getSIV();
    out.fix_type    = _gnss.getFixType();
    out.fix_ok      = _gnss.getGnssFixOk();

    return true;
}

