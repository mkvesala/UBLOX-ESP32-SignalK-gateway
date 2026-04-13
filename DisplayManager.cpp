#include "DisplayManager.h"

// === P U B L I C ===

DisplayManager::DisplayManager(UBLOXProcessor &processorRef, SignalKBroker &signalkRef)
    : _processor(processorRef)
    , _signalk(signalkRef)
{}

// Detect and initialise LCD
void DisplayManager::begin() {
    if (i2cPresent(ADDR_PRI)) {
        _lcd = &_lcd_27;
    } else if (i2cPresent(ADDR_ALT)) {
        _lcd = &_lcd_3f;
    }

    if (_lcd) {
        _lcd->init();
        _lcd->backlight();
        _lcd_present = true;
        printLines("GNSS Gateway", "Initialising...");
    }
}

// Call from Application::loop() — updates display state
void DisplayManager::handle() {
    if (!_lcd_present) return;

    unsigned long now = millis();

    // Show queued message if one is pending or currently displaying
    if (_showing_msg) {
        if ((long)(now - _msg_start_ms) >= MSG_SHOW_MS) {
            _showing_msg = false;
            // Advance queue head
            _queue[_q_head].used = false;
            _q_head = (_q_head + 1) % MSG_QUEUE;
        }
        return; // keep showing until time expires
    }

    // Check if a new message is waiting
    if (_queue[_q_head].used) {
        printLines(_queue[_q_head].l1, _queue[_q_head].l2);
        _showing_msg  = true;
        _msg_start_ms = now;
        return;
    }

    // No queued message — show live GPS data
    showGpsData();
}

// Push a transient message to the display queue
void DisplayManager::showInfoMessage(const char* l1, const char* l2) {
    uint8_t next = (_q_tail + 1) % MSG_QUEUE;
    if (next == _q_head) return; // queue full — drop silently
    copy16(_queue[_q_tail].l1, l1);
    copy16(_queue[_q_tail].l2, l2);
    _queue[_q_tail].used = true;
    _q_tail = next;
}

// Show heap and stack watermark as a transient message
void DisplayManager::showDiagData(uint32_t heap_free, uint32_t stack_wm) {
    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "HEAP %luB", (unsigned long)heap_free);
    snprintf(l2, sizeof(l2), "STACK WM %luB",  (unsigned long)stack_wm);
    showInfoMessage(l1, l2);
}

// === P R I V A T E ===

// Render current GPS data on both rows
void DisplayManager::showGpsData() {
    char top[17], bot[17];
    auto d = _processor.getDelta();

    if (!d.fix_ok || !validf(d.lat_deg)) {
        // No fix
        snprintf(top, sizeof(top), "NO FIX  SV:%-3u", d.satellites);
        const char* ft = "---";
        switch (d.fix_type) {
            case 0: ft = "No fix";     break;
            case 1: ft = "Dead reck."; break;
            case 2: ft = "2D fix";     break;
            case 3: ft = "3D fix";     break;
            case 4: ft = "GNSS+DR";    break;
            default: ft = "Unknown";   break;
        }
        snprintf(bot, sizeof(bot), "%-16s", ft);
    } else {
        // Position row: N/S lat + E/W lon, 4 decimal places each
        char lat_c = (d.lat_deg >= 0) ? 'N' : 'S';
        char lon_c = (d.lon_deg >= 0) ? 'E' : 'W';
        snprintf(top, sizeof(top), "%c%07.4f %c%07.4f",
                 lat_c, fabsf(d.lat_deg),
                 lon_c, fabsf(d.lon_deg));

        // Data row: SOG in knots, COG(T) in degrees, satellite count
        if (validf(d.sog_ms) && validf(d.cog_t_rad)) {
            snprintf(bot, sizeof(bot), "%4.1fkn %3d\xB0 SV:%-2u",
                     sogToKnots(d.sog_ms),
                     cogToDeg(d.cog_t_rad),
                     d.satellites);
        } else {
            // Stationary or no COG
            snprintf(bot, sizeof(bot), "%4.1fkn --- SV:%-2u",
                     validf(d.sog_ms) ? sogToKnots(d.sog_ms) : 0.0f,
                     d.satellites);
        }
    }

    printLines(top, bot);
}

// Print two lines — only rewrite changed lines to avoid flicker
void DisplayManager::printLines(const char* l1, const char* l2) {
    if (!_lcd) return;
    char t[17], b[17];
    copy16(t, l1);
    copy16(b, l2);

    if (strcmp(t, _prev_top) != 0) {
        _lcd->setCursor(0, 0);
        _lcd->print(t);
        for (int i = (int)strlen(t); i < LCD_COLS; i++) _lcd->print(' ');
        copy16(_prev_top, t);
    }
    if (strcmp(b, _prev_bot) != 0) {
        _lcd->setCursor(0, 1);
        _lcd->print(b);
        for (int i = (int)strlen(b); i < LCD_COLS; i++) _lcd->print(' ');
        copy16(_prev_bot, b);
    }
}

bool DisplayManager::i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

void DisplayManager::copy16(char* dst, const char* src) {
    strncpy(dst, src, 16);
    dst[16] = '\0';
}
