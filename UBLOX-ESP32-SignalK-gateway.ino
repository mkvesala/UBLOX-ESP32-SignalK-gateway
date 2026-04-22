#include <Arduino.h>
#include "UBLOXApplication.h"

// === M A I N  P R O G R A M ===
//
// - Owns one UBLOXApplication instance (stack allocated)
// - Delegates all work to app.begin() / app.loop()

UBLOXApplication app;

void setup() {
    Serial.begin(115200);
    delay(47);

    app.begin();

    if (!app.sensorOk()) {
        // Serial.println("[MAIN] SENSOR INIT FAILED — CHECK WIRING!");
        while (1) delay(1999);
    }

    // Serial.println("[MAIN] Setup complete");
}

void loop() {
    app.loop();
}
