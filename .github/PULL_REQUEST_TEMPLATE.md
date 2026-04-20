## Description

Please include a summary of the change and which issue is fixed.

Fixes # (issue)

## Type of change

- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update

## How Has This Been Tested?

Please describe the tests that you ran to verify your changes.

- [ ] Tested on actual hardware
- [ ] ESP32 boots without errors
- [ ] UBLOX MAX-M10S detected on I2C, fix acquired outdoors
- [ ] navigation.position, speedOverGround, courseOverGroundTrue update in SignalK Data Browser
- [ ] navigation.magneticVariation published and plausible for current location
- [ ] Wi-Fi connects
- [ ] SignalK WebSocket works
- [ ] ESP-NOW broadcast transmits GnssDelta packets
- [ ] LCD displays correctly (if connected)
- [ ] OTA updates successful
- [ ] No memory leaks observed

**Test Configuration**:
- ESP32 board: [e.g. ESP32 Dev Module]
- Arduino IDE: [e.g. 2.3.6]
- ESP32 package: [e.g. 3.3.5]

## Checklist:

- [ ] My code follows the style guidelines of this project
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I have made corresponding changes to the documentation
- [ ] My changes generate no new warnings
- [ ] I have tested on actual hardware if possible
- [ ] I have acknowledged any AI assistance in code comments/PR description

## AI Assistance

- [ ] No AI tools used
- [ ] ChatGPT used for code generation/review
- [ ] Claude used for code generation/review
- [ ] GitHub Copilot used
- [ ] Other AI tools (specify): _____________

If AI was used, describe the prompts and how you verified the code:
```
(describe here)
```

