# Contributing to UBLOX-ESP32-SignalK-gateway

Thank you for considering contributing to this project! This document provides guidelines for contributing.

## How Can I Contribute?

### About the project

This is an individual *learning project* by me. Please refer to [README.md](README.md) for the purpose of the project. While new feature suggestions and bug fixes are welcome, the most precious are any advice that might help me improve my knowledge in ESP32 programming.

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates. When creating a bug report, include:

- Clear description of the issue
- Detailed steps to reproduce the behavior
- What you expected to happen
- What actually happened
- Environment:
  - Arduino IDE version
  - ESP32 board package version
  - Hardware used (ESP32 board, UBLOX MAX-M10S module, antenna)
  - Library versions
- Output (Serial monitor, LCD)
- Screenshots, if applicable

### Suggesting Features

Feature requests are welcome! Please:

- Use a clear and descriptive title
- Provide detailed description of the proposed feature
- Explain why this feature would be useful
- Include code examples or mockups if applicable

### Pull Requests

1. **Fork the repository** and create your branch from `main`
   ```
   git checkout -b feature/your-feature-name
   ```
2. Follow the coding style used in the project:
   - Use descriptive variable and function names in English
   - Add comments for complex logic in English
   - Keep class responsibilities clear and focused
   - Follow existing naming conventions
3. Test your changes:
   - Compiles without errors or warnings
   - Test on actual hardware, out there if possible
   - Check that existing features work
4. Update documentation:
   - Update README.md if you add new features
5. Commit your changes:
   ```
   git commit -m "Add brief description of your changes"
   ```
   - Use clear commit messages
6. Push to your fork and submit a PR:
   ```
   git push origin feature/your-feature-name
   ```
7. Wait for maintainer's review and feedback

## Coding guidelines

1. Use consistent indentation
2. Opening brace on the same line for functions and control structures:
  ```
   void myFunction() {
      if (condition) {
         // code
      }
   }
   ```
3. No braces if one line of code like in:
   ```
   if (condition) return;
   ```
4. Naming:
   - Classes `PascalCase`
   - Functions/methods `camelCase`
   - Variables `snake_case`
   - Static variables `s_snake_case`
   - Private members `_snake_case`
   - Constants `UPPER_SNAKE_CASE`
   - Timers: use `_ms`, `_MS`, `_us` to indicate the units for example `NEXT_TRY_MS`
5. Use `//` in single line comments
6. Avoid `#DEFINE` macros, use compile time `static constexpr` when possible
7. Avoid String variables
8. Use `delay()` in loop task *only* when absolutely necessary
9. Minimize dynamic memory allocation - prefer stack allocation
10. Respect ESP32 limited resources and Core 0/Core 1 task responsibilities

## Testing checklist

* [ ] ESP32 boots without errors
* [ ] UBLOX MAX-M10S detected on I2C (0x42); LCD shows fix status and satellite count
* [ ] GPS fix acquired outdoors; `fix_ok`, fix type, and satellite count visible on LCD and `/status` endpoint
* [ ] `navigation.position` updates in SignalK Data Browser when moving > ~1.1 m
* [ ] `navigation.position` heartbeat fires every ~1 s even when stationary
* [ ] `navigation.speedOverGround` updates correctly (SI units, m/s)
* [ ] `navigation.courseOverGroundTrue` updates when moving; absent when SOG < 0.3 m/s
* [ ] `navigation.magneticVariation` published and plausible for current location
* [ ] LCD displays fix status, position, SOG/COG (if present)
* [ ] Wi-Fi connects and reports a valid IP address
* [ ] SignalK WebSocket connection established; server lists ESP32 source in Data Browser
* [ ] ESP-NOW broadcast transmits `GnssDelta` packets (verify with a receiver ESP32)
* [ ] Wi-Fi fallback works: device continues ESP-NOW broadcast when Wi-Fi is unavailable
* [ ] AP intrusion detection fires on unexpected connection attempt: Serial log shows MAC, display shows alert
* [ ] WebSocket reconnection with exponential backoff works after server restart
* [ ] OTA updates are successful
* [ ] No memory leaks (monitor heap via `/status` JSON endpoint)
* [ ] FreeRTOS task stack watermarks stay in reasonable limits (monitor via `/status`)

## Development environment

### Software requirements

[README.md](README.md) for details.

- Arduino IDE 2.3.8
- ESP32 board package 3.3.8
- Required libraries
- SignalK server 2.24.0

### Hardware requirements

[README.md](README.md) for details.

- ESP32 development board (Wemos D1 R32 or equivalent)
- UBLOX MAX-M10S GNSS module with active antenna (I2C, address 0x42)
- (Optional) LCD 16x2 display with I2C backpack

## AI-assisted development

### Policy

1. **Using AI tools is allowed and encouraged** - ChatGPT, Claude Code, Github Copilot or IDE AI copilots, whatever works for you
2. **Transparency is important**  - be open and mention that AI has assisted you in development
3. **Understanding first** - you are responsible for not only the code itself but also being able to explain it

### Best practises

1. **Always test** 
2. **Check security** 
3. **Match coding style** 
4. **Share learnings** 
5. **Put effort on prompting and building skills**
6. **Document skills or other assets in use**

### Copyright

AI tools are assistive tools. You are responsible for the code. Make your best to ensure that the code does not violate 3rd party licenses.

## Code of conduct

1. Check existing issues
2. Create a new issue with your question
3. Be patient - I'm maintaining this on my freetime among other boat projects
4. Be respectful and constructive
5. Welcome beginners
6. No harassment, no inappropriate behaviour, no crimes
