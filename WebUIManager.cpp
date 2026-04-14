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
    // skeleton
}

// Dispatch pending HTTP requests — call from loop()
void WebUIManager::handleRequest() {
    // skeleton
}


