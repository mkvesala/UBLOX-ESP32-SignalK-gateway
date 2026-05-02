# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-04-19

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
  `navigation.courseOverGroundTrue`, `navigation.magneticVariation`,
  `navigation.gnss.satellites`, `navigation.gnss.type`, `navigation.gnss.methodQuality`
- `navigation.gnss.type` is constant `"Combined GPS+GLONASS"` (MAX-M10S is multi-constellation)
- `navigation.gnss.methodQuality` maps UBX fix_type to SignalK enum (`"no GPS"`, `"Estimated (DR) mode"`, `"GNSS Fix"`)
- `navigation.gnss.satellites` and GNSS status fields sent with every transmission (no separate deadband)
- Deadband filtering: position ~0.5 m, SOG 0.025 m/s (~0.05 kn), COG 0.1°, declination 0.1°
- Heartbeat: position and magnetic variation published at least every 0.5 s regardless of deadband
- WebSocket reconnection with exponential backoff (2 s → 120 s)
- Source name derived from ESP32 MAC address: `esp32.ublox-xxxxxx`

**ESP-NOW output**
- Broadcasts `ESPNowPacket<GnssDelta>` to all listeners
- Same deadband and 1 s heartbeat logic as SignalK output
- Works without WiFi association (ESP-NOW operates below the WiFi layer)
- Packet format: shared `espnow_protocol.h` (header + payload, 24 bytes)

**AP interface security — three lines of defence**
- 1st: hidden SSID (`ssid_hidden=1`)
- 2nd: WPA2 password (`AP_PASS`, min. 8 characters)
- 3rd: AP intrusion detection — `ARDUINO_EVENT_WIFI_AP_STACONNECTED` deauths the
  intruder immediately at ESP-IDF level, logs MAC address, and shows a display alert

**Web UI**
- HTTP server on port 80
- OTA updates (ArduinoOTA, password protected)
- `/status` HTML endpoint (no auth): uptime, free heap, min heap since boot, stack watermark, GNSS data, SignalK connection state; auto-refreshes every 5 s
- `WEB_UI_ENABLED` compile-time flag in `UBLOXApplication.h` — set to `false` to disable the HTTP server entirely without removing the implementation
- Session-based authentication and full configuration UI reserved for future release

**Other**
- NVS settings (UBLOXPreferences): configuration persisted to ESP32 flash
- LCD display (I2C, LiquidCrystal_I2C): shows status, position, and diagnostics
- WiFi state machine with automatic reconnection
- `WIFI_AP_STA` mode enables concurrent ESP-NOW and WiFi-STA operation
- Serial diagnostics every ~30 s: free heap, min heap since boot, stack watermark, magnetic variation; reset reason logged at boot

### Fixed

- **`initWifiServices()` called on every WiFi reconnect** — `ArduinoOTA.begin()` and `SignalKBroker::begin()` were invoked again on each WiFi reconnect, which can destabilise OTA and cause double WebSocket connect attempts; added `_wifi_services_started` guard so they run only once
- **`navigation.gnss.satellites` and `navigation.gnss.methodQuality` not updating when stationary** — these fields were only transmitted when position/SOG/COG/magvar triggered a send; added `ch_sat` deadband tracking so a change in satellite count or fix type independently triggers transmission

[1.0.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.0.0
