#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// DisplayManager
//
// Drives a 128×64 SSD1306 OLED via I²C using the U8g2 library.
// Three display pages cycle automatically; the caller can also force a page.
// All rendering is double-buffered — call refresh() to push the frame.
// ---------------------------------------------------------------------------
class DisplayManager {
public:
    DisplayManager() = default;

    /** Initialise I²C bus and OLED controller. */
    void begin();

    /**
     * Update internal data snapshots used for rendering.
     * Call this whenever fresh data is available (no rendering overhead).
     */
    void updateGps(const GPSRecord& rec);
    void updateBattery(const BatteryStatus& batt);
    void updateModem(const ModemStatus& modem);

    /**
     * Auto-advance the page if DISPLAY_PAGE_CYCLE_MS has elapsed, then
     * render the current page and flush to the display.
     */
    void refresh();

    /** Immediately jump to a specific page. */
    void setPage(DisplayPage page);

    /** Dim or turn off the display to save power. */
    void sleep();
    void wake();

private:
    // U8g2 constructor for SSD1306 128×64, hardware I²C, full-buffer mode.
    // Change the first parameter to U8G2_SH1106_128X64_NONAME_F_HW_I2C for SH1106 panels.
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2{U8G2_R0, /* reset= */ U8X8_PIN_NONE};

    DisplayPage   _currentPage    = DisplayPage::STATUS;
    uint32_t      _lastPageSwitch = 0;
    bool          _sleeping       = false;

    // Latest snapshots
    GPSRecord     _gpsSnap{};
    BatteryStatus _battSnap{};
    ModemStatus   _modemSnap{};

    // Page renderers
    void _drawStatus();
    void _drawLocation();
    void _drawStats();

    // Helpers
    void _drawBatteryIcon(uint8_t x, uint8_t y, uint8_t pct, bool charging);
    void _drawSignalBars(uint8_t x, uint8_t y, int8_t rssi);
};
