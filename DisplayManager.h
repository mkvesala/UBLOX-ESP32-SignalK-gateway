#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "UBLOXProcessor.h"
#include "SignalKBroker.h"
#include "helpers.h"

// === D I S P L A Y M A N A G E R ===
//
// - LCD 16×2 display via I2C (0x27 or 0x3F)
// - Normal: position + SOG/COG/satellites
// - No fix: status message
// - FIFO queue for info/debug messages (shown 2 s, then back to GPS data)
// - Owned by: UBLOXApplication

class DisplayManager {
public:
    explicit DisplayManager(UBLOXProcessor &processorRef, SignalKBroker &signalkRef);

    void begin();
    void handle();

    // Show a transient message (2 s) — pushed to FIFO queue
    void showInfoMessage(const char* l1, const char* l2);

    // Show heap / watermark diagnostics on next free slot
    void showDiagData(uint32_t heap_free, uint32_t stack_wm);

    bool isPresent() const { return _lcd_present; }

private:
    static constexpr uint8_t ADDR_PRI  = 0x27;
    static constexpr uint8_t ADDR_ALT  = 0x3F;
    static constexpr uint8_t LCD_COLS  = 16;
    static constexpr uint8_t LCD_ROWS  = 2;
    static constexpr uint8_t MSG_QUEUE = 3;
    static constexpr unsigned long MSG_SHOW_MS = 2000;

    UBLOXProcessor &_processor;
    SignalKBroker  &_signalk;

    LiquidCrystal_I2C _lcd_27 {ADDR_PRI, LCD_COLS, LCD_ROWS};
    LiquidCrystal_I2C _lcd_3f {ADDR_ALT, LCD_COLS, LCD_ROWS};
    LiquidCrystal_I2C* _lcd = nullptr;
    bool _lcd_present = false;

    // FIFO message queue
    struct Msg {
        char l1[17];
        char l2[17];
        bool used = false;
    };
    Msg _queue[MSG_QUEUE];
    uint8_t _q_head = 0;
    uint8_t _q_tail = 0;

    bool         _showing_msg = false;
    unsigned long _msg_start_ms = 0;

    char _prev_top[17] {};
    char _prev_bot[17] {};

    void showGpsData();
    void printLines(const char* l1, const char* l2);

    static bool  i2cPresent(uint8_t addr);
    static void  copy16(char* dst, const char* src);
    static float sogToKnots(float ms) { return ms * 1.94384f; }
    static int   cogToDeg(float rad)  { return (int)(rad * RAD_TO_DEG + 0.5f); }
};
