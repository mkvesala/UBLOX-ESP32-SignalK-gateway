#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "UBLOXProcessor.h"
#include "UBLOXPreferences.h"
#include "SignalKBroker.h"
#include "DisplayManager.h"

// === W E B U I M A N A G E R ===
//
// - HTTP server: /status debug endpoint (no auth)
// - Authentication and full UI to be added later (see CMPS14 reference)
// - Owned by: UBLOXApplication
// - Uses: UBLOXProcessor, UBLOXPreferences, SignalKBroker, DisplayManager
// - Owns: WebServer


class WebUIManager {

public:

    explicit WebUIManager(
        UBLOXProcessor &processorRef,
        UBLOXPreferences &prefsRef,
        SignalKBroker &signalkRef,
        DisplayManager &displayRef
    );

    void begin();
    void handleRequest();

private:
    WebServer _server {80};
    UBLOXProcessor &_processor;
    UBLOXPreferences &_prefs;
    SignalKBroker &_signalk;
    DisplayManager &_display;

    void handleStatus();

};
