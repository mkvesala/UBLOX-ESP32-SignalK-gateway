# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2026-09-06

### Added
- **GNSS UTC published on both transports** — the gateway now broadcasts the
  `DATETIME_DELTA` (9) message type of `espnow_protocol.h` and publishes the same UTC
  to SignalK as `navigation.datetime`. Both go out at ~2 s
  intervals from a single new `handleDateTime()` handler. The ESP-NOW leg has no WiFi
  guard, so receivers keep a correct clock while the gateway is offline; the SignalK
  leg is skipped unless the WebSocket is connected. This lets displays with neither an
  RTC nor NTP — ESP32-Crowpanel-SkippersWatch's clock page — set their system clock
  straight from GNSS.
- **UTC epoch read from the module** — `RawGnssData` gains `unix_utc` and `time_valid`.
  The epoch is read via `getUnixEpoch()` and rounded to the nearest second from that
  call's microsecond out-parameter, so a receiver's clock lands within ±0.5 s instead
  of carrying a systematic 0…−1 s lag. Validity is `getConfirmedTime() &&
  getDateValid()`, exactly as the shared protocol header specifies.
- **`UBLOXProcessor::DateTimeDelta`** — UTC is kept in its own struct rather than
  inside `GnssDelta`, because the module can confirm the time before a position fix is
  available and `updateDelta()` NANs `GnssDelta` whenever the fix is lost. Both brokers
  therefore keep publishing time through a fix outage. The value is accepted only above
  a 2020-01-01 epoch floor and is sticky once good, so a momentary loss of confirmation
  never blanks a clock that receivers have already synchronised to.

### Notes
- `navigation.datetime` is an RFC 3339 UTC **string** ending in `Z`, per the SignalK
  JSON schema (`schemas/groups/navigation.json`, pattern `.*Z$`) — the prose
  specification does not state the format. The schema's `gnssTimeSource` is a sibling
  of `value` rather than a leaf and so cannot be carried in a delta path; the existing
  `navigation.gnss.type` conveys the same information.
- **Shared `espnow_protocol.h` gains the `DATETIME_DELTA` (9) allocation** — the enum
  value, its row in the fleet-wide `msg_type` table, and the 8-byte `DateTimeDelta`
  payload struct. The change is purely additive: no existing enum value or payload
  struct was modified, so the wire format is unchanged and every existing receiver
  stays compatible. All fleet projects — BME280, CMPS14, DFWind, HALMET, VEDirect,
  SignalK-ESP-NOW-gateway, ESP32-Crowpanel-compass and ESP32-Crowpanel-SkippersWatch —
  already carry the byte-identical header, so no further cross-project copy is due.
- **`getUnixEpoch()` must be called last, and only above year 2020** — it clears the
  year/month/day/hour/min/sec `moduleQueried` bits, so reading it before the other
  getters would make each of them re-issue a blocking `getPVT()`. The `year >= 2020`
  guard is mandatory rather than cosmetic: the library indexes
  `SFE_UBLOX_DAYS_SINCE_2020[year - 2020]` with no bounds check, so an unfixed module
  reporting year 0 would read out of bounds and return garbage.

## [1.2.1] - 2026-07-29

### Changed
- **Shared `espnow_protocol.h` updated to the fleet-wide superset** — adds the
  `HALMET_WATER_DELTA` (7) and `DEPTH_DELTA` (8) message types with their payload
  structs, and documents the shared-header ownership rules together with the
  fleet-wide `msg_type` allocation. No existing enum value or payload struct was
  modified, so the wire format is unchanged and every existing receiver stays
  compatible. This gateway's own firmware logic is untouched.

### Fixed
- **Position and SOG hidden at anchor on ESP-NOW receivers** — the receiver-side
  `convertGnssDeltaToData()` folded a NaN COG into `fix_ok`, so a stationary boat —
  which has a valid position fix but an undefined course — appeared to have no fix at
  all and the display suppressed position and SOG, exactly the anchored-glance case
  the watch exists for. `GnssData` now tracks position validity (`fix_ok`) and course
  validity (`cog_valid`) separately, exposed as `hasFix()` / `hasCog()`, and carries
  `lat_deg` / `lon_deg` for the receiver's position display. Receiver-side only — the
  data this gateway transmits is unchanged.

## [1.2.0] - 2026-07-16

### Changed
- **WebSocket client recreated on every reconnect** — `SignalKBroker` now owns the
  `WebsocketsClient` via `std::unique_ptr` and builds a fresh instance in
  `connectWebsocket()` (registering callbacks before `connect()`), destroying it in
  `closeWebsocket()` (`_ws.reset()`). This runs the `WiFiClient` destructor and frees
  the underlying lwIP socket fd, guaranteeing a clean transport on every attempt.
  Fixes a rare lock-up where, after ~12–48 h, the SignalK connection could not be
  re-established without a manual reboot even though WiFi, heap and the main loop were
  healthy — a stuck/leaked socket was inherited by every subsequent `connect()`.
  All teardown paths (including the `sendDelta()` send-failure) now funnel through
  `closeWebsocket()`. Public API and reconnect/backoff/ping-pong logic are unchanged.

## [1.1.0] - 2026-07-04

### Added
- **WebSocket liveness (ping/pong) + graceful reconnect** — `SignalKBroker` now
  detects a half-open TCP connection where the socket still reports open but no
  data flows (e.g. macOS power-save freezing the SignalK server). While the
  socket is open, `UBLOXApplication` sends a client ping every ~10 s
  (`WS_PING_MS`); `SignalKBroker` records each server pong and exposes
  `isStale(now)` (no pong within `PONG_TIMEOUT_MS`, ~30 s). A stale socket is
  closed and re-established through the existing exponential-backoff path — no
  `ESP.restart()`. New `SignalKBroker::ping()` / `SignalKBroker::isStale()`.
- **Static IP option (default)** — `applyStaticIP()` configures a fixed address
  from `secrets.h` (`WIFI_STATIC_IP` / `WIFI_GATEWAY` / `WIFI_SUBNET`), removing
  the dependency on the router's DHCP lease.

### Changed
- **Hardened WiFi reconnect** — on link loss the `WifiState::CONNECTED` branch now
  closes the websocket, does a clean STA teardown (`WiFi.disconnect(true)`),
  reapplies `setSleep(false)` and the static IP, then reconnects. Recovers from
  the macOS power-save network freeze that previously required a manual reboot.
  The `CONNECTING → CONNECTED` transition also resets `_next_ws_try_ms` so the
  websocket reconnects immediately after WiFi recovers.

## [1.0.1] - 2026-05-07

### Changed
- Updated `POS_HEARTBEAT_MS` constant of `ESPNowBroker` to 499 ms and of `SignalKBroker` to 503 ms.
- Updated `WIFI_STATUS_CHECK_MS` constant of `UBLOXApplication` to 491 ms
- Commented out all Serial outputs in the project

## [1.0.0] - 2026-05-07

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

[1.3.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.3.0
[1.2.1]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.2.1
[1.2.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.2.0
[1.1.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.1.0
[1.0.1]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.0.1
[1.0.0]: https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway/releases/tag/v1.0.0
