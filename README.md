# UBLOX-ESP32-SignalK Gateway

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/en/sdks/esp-arduino)
[![Sensor: UBLOX MAX-M10S](https://img.shields.io/badge/Sensor-UBLOX%20MAX--M10S-lightgrey)](https://www.u-blox.com/en/product/max-m10-series)
[![Server: SignalK](https://img.shields.io/badge/Server-SignalK-orange)](https://signalk.org)
[![Protocol: WebSocket](https://img.shields.io/badge/Protocol-WebSocket-red)](https://github.com/gilmaimon/ArduinoWebsockets)
[![Protocol: ESP-NOW](https://img.shields.io/badge/Protocol-ESP--NOW-red)](https://www.espressif.com/en/solutions/low-power-solutions/esp-now)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

ESP32-based GNSS gateway reading position, speed over ground, and course over ground from a [UBLOX MAX-M10S](https://www.u-blox.com/en/product/max-m10-series) module. Sends navigation data to a [SignalK](https://signalk.org) server via WebSocket/JSON and to other ESP32 devices via ESP-NOW broadcast. Magnetic variation is computed from GPS position and date using the WMM_Tinier geomagnetic model.

OTA firmware updates are enabled. Configuration is persisted to NVS. A web UI provides a status endpoint and password-protected configuration interface.

Developed and tested on:
- [Wemos D1 R32 ESP32 development board](https://partco.fi/tuote/arduino-esp32-kehityskortti-esp-wroom-32-2526)
- [ESP32 board package](https://github.com/espressif/arduino-esp32) (3.3.8)
- [Arduino IDE](https://www.arduino.cc/en/software/) (2.3.8)
- SignalK Server (2.24.0)
- [SparkFun GNSS Receiver Breakout MAX-M10S](https://www.sparkfun.com/sparkfun-gnss-receiver-breakout-max-m10s-qwiic.html)

## Purpose of the project

This is one of my individual digital boat projects. Use at your own risk. Not for safety-critical operations.

1. I needed accurate GPS position, SOG, and COG available in SignalK and on the vessel's ESP-NOW peer-to-peer network
2. The UBLOX MAX-M10S does not natively report magnetic variation — the WMM_Tinier model fills that gap entirely
3. I wanted to continue building on the ESP32 gateway design pattern established in previous projects

## Release history

| Release | Branch | Comment |
|---------|--------|---------|
| v0.1.0 | main | Initial release. GNSS reading, WMM magnetic variation, SignalK WebSocket, ESP-NOW broadcast, AP intrusion detection, LCD display. |

## Classes

Class diagram including the companion projects:

<img src="https://raw.githubusercontent.com/mkvesala/ESP32-Crowpanel-compass/main/docs/full_uml_diagram.jpeg" width="480">

**`UBLOXSensor`:**
- Uses: `SFE_UBLOX_GNSS`, `TwoWire`
- Owned by: `UBLOXApplication`
- Responsible for: I2C communication with the MAX-M10S module; exposes raw PVT data with no processing

**`UBLOXProcessor`:**
- Owns: `GnssDelta` (data struct), `WMM_Tinier`
- Uses: `UBLOXSensor`
- Owned by: `UBLOXApplication`
- Responsible for: unit conversion, COG gating, magnetic variation computation via WMM model

**`UBLOXPreferences`:**
- Owns: `Preferences`
- Uses: `UBLOXProcessor`
- Owned by: `UBLOXApplication`
- Responsible for: loading and saving configuration to ESP32 NVS

**`SignalKBroker`:**
- Owns: `WebsocketsClient`
- Uses: `UBLOXProcessor`
- Owned by: `UBLOXApplication`
- Responsible for: WebSocket connection and delta transmission to SignalK server

**`ESPNowBroker`:**
- Uses: `UBLOXProcessor`
- Owned by: `UBLOXApplication`
- Responsible for: ESP-NOW broadcast of GNSS navigation data

**`DisplayManager`:**
- Owns: `LiquidCrystal_I2C`
- Uses: `UBLOXProcessor`, `SignalKBroker`
- Owned by: `UBLOXApplication`
- Responsible for: optional LCD 16x2 display

**`WebUIManager`:**
- Owns: `WebServer`
- Uses: `UBLOXProcessor`, `UBLOXPreferences`, `SignalKBroker`, `DisplayManager`
- Owned by: `UBLOXApplication`
- Responsible for: HTTP web user interface with session authentication

**`UBLOXApplication`:**
- Owns: `UBLOXSensor`, `UBLOXProcessor`, `UBLOXPreferences`, `SignalKBroker`, `ESPNowBroker`, `DisplayManager`, `WebUIManager`
- Uses: `WifiState`
- Responsible for: orchestrating all subsystems; acts as "the app"

**`WifiState`:**
- Global enum class for WiFi states maintained and shared by `UBLOXApplication`

## Features

### GNSS reading

1. Reads position (lat/lon), speed over ground, course over ground, satellite count, and fix type from UBLOX MAX-M10S over I2C at 6 Hz
2. COG is gated: published only when SOG ≥ 0.3 m/s (~0.6 kn) to suppress noise when stationary
3. Magnetic variation is computed each cycle from GPS position and UTC date using the WMM_Tinier geomagnetic model

### SignalK communication

Connects to:
```
ws://<server>:<port>/signalk/v1/stream?token=<optional>
```

**Sends** at ~5 Hz with deadband filtering:

| SignalK path | Unit | Notes |
|---|---|---|
| `navigation.position` | decimal degrees | Deadband ~1.1 m; heartbeat every 10 s regardless |
| `navigation.speedOverGround` | m/s | Deadband 0.05 m/s (~0.1 kn) |
| `navigation.courseOverGroundTrue` | radians | Deadband 0.5°; only when SOG ≥ 0.3 m/s |
| `navigation.magneticVariation` | radians | Deadband 0.5°; computed via WMM_Tinier |

Source name is auto-derived from the device MAC address: `esp32.ublox-XXYYZZ`.

WebSocket reconnects automatically with exponential back-off starting at ~2 s, doubling on each failed attempt up to a ceiling of ~120 s, and resetting to the initial interval when the connection is restored.

### ESP-NOW communication

Broadcasts GNSS data via ESP-NOW for other ESP32 devices, such as external displays (e.g. ESP32-Crowpanel-compass).

**Sends** at ~5 Hz with the same deadband and heartbeat logic as SignalK output:
- `GnssDelta` struct (24 bytes) containing:
  - `lat_deg`, `lon_deg` — position in decimal degrees
  - `sog_ms` — speed over ground in m/s
  - `cog_true_rad` — COG true in radians
  - `mag_var_rad` — magnetic variation in radians

**Broadcast mode:** Uses broadcast address (FF:FF:FF:FF:FF:FF) — any ESP-NOW receiver on the same WiFi channel can listen.

**WiFi coexistence:** ESP-NOW operates alongside WiFi in `WIFI_AP_STA` mode. Both SignalK WebSocket and ESP-NOW broadcast function simultaneously.

**Note: ESP-NOW receivers must be on the same WiFi channel as this device. The simplest approach is to connect both devices to the same WiFi network with a fixed channel.**

### AP interface security — three lines of defence

The `WIFI_AP_STA` mode opens a local AP interface required for ESP-NOW. This interface is locked down in three layers:

1. **Hidden SSID** — the AP does not broadcast its network name
2. **WPA2 password** — connection requires `AP_PASS` (minimum 8 characters)
3. **AP intrusion detection** — any successful connection attempt triggers an immediate ESP-IDF level deauth, Serial log with the intruder's MAC address, and a display alert

### LCD 16x2 display

Shows live GNSS status, position, SOG/COG, fix type, and satellite count.
LCD is auto-detected at startup — device boots normally if no display is connected.

### WiFi and OTA

- WiFi state machine: `INIT → CONNECTING → CONNECTED`, with a 90-second connection timeout and automatic fallback to `OFF` on failure or missing SSID
- Auto-reconnect on dropped connection
- ArduinoOTA enabled immediately after WiFi connects; hostname is set to the SignalK source name
- Without WiFi the device still broadcasts via ESP-NOW at full rate

## Project structure

| File(s) | Description |
|---------|-------------|
| `UBLOX-ESP32-SignalK-gateway.ino` | Owns `UBLOXApplication app`, contains `setup()` and `loop()` |
| `secrets.example.h` | Example credentials. Rename to `secrets.h` and populate. |
| `version.h` | Software version |
| `WifiState.h` | Enum class for WiFi states |
| `espnow_protocol.h` | Shared ESP-NOW wire protocol — header, packet template, all payload structs |
| `helpers.h` | `validf()` float validator, angular difference helper |
| `UBLOXSensor.h / .cpp` | Class `UBLOXSensor` — raw I2C communication with MAX-M10S |
| `UBLOXProcessor.h / .cpp` | Class `UBLOXProcessor` — unit conversion, COG gating, WMM magnetic variation |
| `UBLOXPreferences.h / .cpp` | Class `UBLOXPreferences` — NVS configuration storage |
| `SignalKBroker.h / .cpp` | Class `SignalKBroker` — WebSocket delta transmission |
| `ESPNowBroker.h / .cpp` | Class `ESPNowBroker` — ESP-NOW broadcast |
| `DisplayManager.h / .cpp` | Class `DisplayManager` — LCD 16x2 display |
| `WebUIManager.h / .cpp` | Class `WebUIManager` — HTTP web UI with session authentication |
| `UBLOXApplication.h / .cpp` | Class `UBLOXApplication` — top-level orchestrator |

## Hardware

### Bill of materials

1. Wemos R1 D32 or equivalent ESP32 development board
2. Sparkfun UBLOX MAX-M10S GNSS module with active antenna (I2C, address 0x42)
3. LCD 16x2 module with I2C backpack (optional, address 0x27)
4. Wiring and connectors
5. Enclosure, DC power supply
6. WiFi router providing wireless LAN AP
7. SignalK server running in LAN

**No paid partnerships.**

## Software used

1. Arduino IDE 2.3.8
2. Espressif Systems esp32 board package 3.3.8
3. Additional libraries installed:
   - SparkFun u-blox GNSS Arduino Library v3 (by SparkFun Electronics, version 3.1.13)
   - ArduinoWebsockets (by Gil Maimon, version 0.5.4)
   - ArduinoJson (by Benoit Blanchon, version 7.4.3)
   - WMM_Tinier (by David Armstrong, version 1.0.3)
   - LiquidCrystal_I2C (by Frank de Brabander, version 1.1.2)

## Installation

1. Clone the repo
   ```
   git clone https://github.com/mkvesala/UBLOX-ESP32-SignalK-gateway.git
   ```
2. Alternatively, download the code as zip
3. Set up your credentials in `secrets.h` (first by renaming `secrets.example.h` to `secrets.h`)
   ```cpp
   inline constexpr const char* WIFI_SSID            = "your_wifi_ssid_here";
   inline constexpr const char* WIFI_PASS            = "your_wifi_password_here";
   inline constexpr const char* SK_HOST              = "your_signalk_address_here";
   inline constexpr uint16_t    SK_PORT              = 3000;
   inline constexpr const char* SK_TOKEN             = "your_signalk_auth_token_here";
   inline constexpr const char* OTA_PASS             = "your_OTA_password_here";
   inline constexpr const char* DEFAULT_WEB_PASSWORD = "your_default_web_password_here";
   inline constexpr const char* AP_SSID              = "your_ap_ssid_here";
   inline constexpr const char* AP_PASS              = "your_ap_password_here";  // min 8 chars
   ```
4. **Make sure that `secrets.h` is listed in your `.gitignore` file**
5. Connect the UBLOX MAX-M10S to I2C (SDA GPIO21, SCL GPIO22) and optionally connect an LCD to the same bus
6. Connect and power up the ESP32
7. Compile and upload with Arduino IDE (ESP32 board package and required libraries installed)

## Security

### Maritime navigation

**Use at your own risk — not for safety-critical operations!**

### Important security considerations

1. **HTTP only (no HTTPS)**
   - Use only on private, trusted networks

2. **LAN deployment only**
   - Do NOT expose to public internet
   - Keep ESP32 on isolated WiFi
   - Use WPA2/WPA3 encryption

3. **SignalK token visibility**
   - SignalK authentication token is visible in the WebSocket URL
   - Keep ESP32 and SignalK server on the same private network

4. **`secrets.h`**
   - Make sure that `secrets.h` is listed in your `.gitignore` file

### Deployment

**Recommended:**
- Deploy on private isolated boat WiFi
- Use WPA2/WPA3 WiFi encryption

**Not recommended:**
- Public internet exposure
- Port forwarding to ESP32
- Sharing WiFi network with untrusted devices

## Credits

Hardware and libraries described earlier in this document.

Developed by Matti Vesala in collaboration with Claude (Anthropic).

Check [CONTRIBUTING.md](CONTRIBUTING.md) for further information on AI-assisted development in the project.

I would highly appreciate improvement suggestions as well as any Arduino-style ESP32/C++ coding advice.

This is a companion project to [CMPS14-ESP32-SignalK-gateway](https://github.com/mkvesala/CMPS14-ESP32-SignalK-gateway), [BME280-ESP32-SignalK-gateway](https://github.com/mkvesala/BME280-ESP32-SignalK-gateway), and [ESP32-Crowpanel-compass](https://github.com/mkvesala/ESP32-Crowpanel-compass). Check the UML diagram to see how these projects relate:

<img src="https://raw.githubusercontent.com/mkvesala/ESP32-Crowpanel-compass/main/docs/full_uml_diagram.jpeg" width="480">
