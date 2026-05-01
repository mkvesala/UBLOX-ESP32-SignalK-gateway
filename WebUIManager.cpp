#include "WebUIManager.h"
#include "version.h"
#include "helpers.h"

// === P U B L I C ===

WebUIManager::WebUIManager(
    UBLOXProcessor &processorRef,
    UBLOXPreferences &prefsRef,
    SignalKBroker &signalkRef,
    DisplayManager &displayRef
) : _processor(processorRef)
  , _prefs(prefsRef)
  , _signalk(signalkRef)
  , _display(displayRef)
{}

// Register routes and start HTTP server
void WebUIManager::begin() {
    _server.on("/status", [this]() { this->handleStatus(); });
    _server.begin();
}

// Dispatch pending HTTP requests — call from loop()
void WebUIManager::handleRequest() {
    _server.handleClient();
}

// === P R I V A T E ===

// Debug status page — no auth, auto-refreshes every 5 s
void WebUIManager::handleStatus() {
    uint32_t heap    = ESP.getFreeHeap();
    uint32_t minheap = ESP.getMinFreeHeap();
    uint32_t wm      = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
    uint32_t uptime  = millis() / 1000;
    auto d = _processor.getDelta();

    char sog[12], cog[12], var[12], lat[16], lon[16];
    if (validf(d.sog_ms))      snprintf(sog, sizeof(sog), "%.2f kn",  d.sog_ms * 1.94384f);
    else                       strcpy(sog, "--");
    if (validf(d.cog_t_rad))   snprintf(cog, sizeof(cog), "%.1f deg", d.cog_t_rad * RAD_TO_DEG);
    else                       strcpy(cog, "--");
    if (validf(d.mag_var_rad)) snprintf(var, sizeof(var), "%.1f deg", d.mag_var_rad * RAD_TO_DEG);
    else                       strcpy(var, "--");
    if (validf(d.lat_deg))     snprintf(lat, sizeof(lat), "%.6f",     d.lat_deg);
    else                       strcpy(lat, "--");
    if (validf(d.lon_deg))     snprintf(lon, sizeof(lon), "%.6f",     d.lon_deg);
    else                       strcpy(lon, "--");

    char buf[768];
    snprintf(buf, sizeof(buf),
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"5\">"
        "<title>UBLOX Debug</title></head><body>"
        "<h2>UBLOX Gateway Debug</h2><pre>"
        "Version:      %s\n"
        "Uptime:       %lu s\n"
        "Free heap:    %lu B\n"
        "Min heap:     %lu B\n"
        "Stack WM:     %lu B\n"
        "\nGNSS:\n"
        "  Fix:        %s (type %d)\n"
        "  Satellites: %d\n"
        "  Lat:        %s\n"
        "  Lon:        %s\n"
        "  SOG:        %s\n"
        "  COG(T):     %s\n"
        "  Mag var:    %s\n"
        "\nSignalK:      %s\n"
        "</pre></body></html>",
        SW_VERSION,
        (unsigned long)uptime,
        (unsigned long)heap,
        (unsigned long)minheap,
        (unsigned long)wm,
        d.fix_ok ? "yes" : "no", d.fix_type,
        d.satellites,
        lat, lon, sog, cog, var,
        _signalk.isOpen() ? "connected" : "disconnected"
    );

    _server.send(200, "text/html", buf);
}
