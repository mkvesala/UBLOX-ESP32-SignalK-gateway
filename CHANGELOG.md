# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-04-19

First working release. Reads UBLOX MAX-M10S GNSS module and forwards navigation
data to both a SignalK server and the ESP-NOW network.

### Added

**GNSS reading**
- UBLOX MAX-M10S over I2C (SparkFun u-blox GNSS v3 library)
- 6 Hz update rate, UBX protocol only, autoPVT
- Reads: position (lat/lon), SOG (m/s), COG(T) (rad), satellite count, fix type
- COG(T) gated: published only when SOG ≥ 0.3 m/s (~0.6 kn)

**Magnetic declination (WMM_Tinier)**
- Computed with WMM_Tinier library from GPS position and date
- Updated every PVT cycle when fix is valid and GPS date is available
- MAX-M10S does not report declination natively — WMM calculation replaces it entirely

**SignalK output (WebSocket)**
- Publishes: `navigation.position`, `navigation.speedOverGround`,
  `navigation.courseOverGroundTrue`, `navigation.magneticVariation`
- Deadband filtering: position ~1.1 m, SOG 0.05 m/s, COG 0.5°, declination 0.5°
- Position heartbeat: position published at least every 10 s regardless of deadband
- WebSocket reconnection with exponential backoff (2 s → 120 s)
- Source name derived from ESP32 MAC address: `esp32.ublox-xxxxxx`

**ESP-NOW output**
- Broadcasts `ESPNowPacket<GnssDelta>` to all listeners
- Same deadband and heartbeat logic as SignalK output
- Works without WiFi association (ESP-NOW operates below the WiFi layer)
- Packet format: shared `espnow_protocol.h` (header + payload, 24 bytes)

**AP interface security — three lines of defence**
- 1st: hidden SSID (`ssid_hidden=1`)
- 2nd: WPA2 password (`AP_PASS`, min. 8 characters)
- 3rd: AP intrusion detection — `ARDUINO_EVENT_WIFI_AP_STACONNECTED` deauths the
  intruder immediately at ESP-IDF level, logs MAC address, and shows a display alert

**Web UI**
- HTTP server on port 80
- Session-based authentication (SHA256 password, 128-bit random token)
- Per-IP login rate limiting
- OTA updates (ArduinoOTA, password protected)
- `/status` JSON endpoint: position, SOG, COG, satellites, declination, heap, version

**Other**
- NVS settings (UBLOXPreferences): configuration persisted to ESP32 flash
- LCD display (I2C, LiquidCrystal_I2C): shows status, position, and diagnostics
- Periodic diagnostics to Serial (~30 s): heap, stack watermark, magvar + source
- WiFi state machine with automatic reconnection
- `WIFI_AP_STA` mode enables concurrent ESP-NOW and WiFi-STA operation

[0.1.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v0.1.0
