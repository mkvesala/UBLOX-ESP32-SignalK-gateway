#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_u-blox_GNSS_v3.h>

// === U B L O X S E N S O R ===
//
// - Responsible only for physical I2C communication with MAX-M10S
// - No data processing, no business logic
// - begin(Wire) initialises connection and configures the module
// - read() fetches the latest PVT data (non-blocking via getPVT(0))
// - Owned by: UBLOXApplication
// - Uses: TwoWire


struct RawGnssData {
    int32_t  lat_e7;       // degrees × 10^-7
    int32_t  lon_e7;       // degrees × 10^-7
    int32_t  speed_mms;    // mm/s
    int32_t  heading_e5;   // degrees × 10^-5 (true north referenced)
    int16_t  mag_dec;      // magnetic variation — scale tbd (verify during debug)
    uint16_t mag_acc;      // variation accuracy estimate — same scale as mag_dec
    uint8_t  siv;          // satellites in view
    uint8_t  fix_type;     // 0=no fix, 3=3D, 4=GNSS+DR
    bool     fix_ok;       // getGnssFixOk()
};

class UBLOXSensor {

public:

    explicit UBLOXSensor();

    bool begin(TwoWire &wirePort);
    bool available() const;

    // Non-blocking PVT read — returns true only when fresh data arrived
    bool read(RawGnssData &out);

    // Direct accessors used by UBLOXProcessor::begin() for magvar probe
    int16_t getMagDec();
    uint16_t getMagAcc();

private:

    static constexpr uint8_t GNSS_ADDR = 0x42;

    SFE_UBLOX_GNSS _gnss;
    TwoWire* _wire = nullptr;

};
