#include "WebUIManager.h"
#include "version.h"
#include "helpers.h"

// === P U B L I C ===

WebUIManager::WebUIManager(
    UBLOXProcessor  &processorRef,
    UBLOXPreferences &prefsRef,
    SignalKBroker   &signalkRef,
    DisplayManager  &displayRef
) : _processor(processorRef)
  , _prefs(prefsRef)
  , _signalk(signalkRef)
  , _display(displayRef)
{}

// Register routes and start HTTP server
void WebUIManager::begin() {
    this->setupRoutes();
    _server.begin();
}

// Dispatch pending HTTP requests — call from loop()
void WebUIManager::handleRequest() {
    _server.handleClient();
}

// === P R I V A T E ===

void WebUIManager::setupRoutes() {
    // Minimal status endpoint — no auth yet
    _server.on("/status", HTTP_GET, [this]() {
        auto d = _processor.getDelta();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"lat\":%.6f,\"lon\":%.6f,\"sog_ms\":%.3f,"
            "\"cog_t_deg\":%.1f,\"siv\":%u,\"fix_type\":%u,"
            "\"fix_ok\":%s,\"version\":\"%s\"}",
            validf(d.lat_deg)   ? d.lat_deg   : 0.0f,
            validf(d.lon_deg)   ? d.lon_deg   : 0.0f,
            validf(d.sog_ms)    ? d.sog_ms    : 0.0f,
            validf(d.cog_t_rad) ? d.cog_t_rad * RAD_TO_DEG : 0.0f,
            d.satellites,
            d.fix_type,
            d.fix_ok ? "true" : "false",
            SW_VERSION
        );
        _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        _server.send(200, "application/json; charset=utf-8", buf);
    });
}
