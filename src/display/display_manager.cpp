#include "display_manager.h"
#include "../../include/config.h"
#include "../../include/pins.h"

// ---------------------------------------------------------------------------

void DisplayManager::begin() {
    _u8g2.setI2CAddress(OLED_I2C_ADDR << 1);  // U8g2 expects 8-bit shifted address
    _u8g2.begin();
    _u8g2.setContrast(DISPLAY_CONTRAST);
    _u8g2.clearBuffer();
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.drawStr(20, 32, "GPS Tracker v" FIRMWARE_VERSION);
    _u8g2.sendBuffer();
    Serial.println(F("[DISPLAY] OLED initialised"));
}

// ---------------------------------------------------------------------------

void DisplayManager::updateGps(const GPSRecord& rec)       { _gpsSnap   = rec;   }
void DisplayManager::updateBattery(const BatteryStatus& b) { _battSnap  = b;     }
void DisplayManager::updateModem(const ModemStatus& m)     { _modemSnap = m;     }

// ---------------------------------------------------------------------------

void DisplayManager::refresh() {
    if (_sleeping) return;

    // Auto-advance page
    if (millis() - _lastPageSwitch >= DISPLAY_PAGE_CYCLE_MS) {
        uint8_t next = (static_cast<uint8_t>(_currentPage) + 1)
                       % static_cast<uint8_t>(DisplayPage::PAGE_COUNT);
        _currentPage    = static_cast<DisplayPage>(next);
        _lastPageSwitch = millis();
    }

    _u8g2.clearBuffer();
    switch (_currentPage) {
        case DisplayPage::STATUS:   _drawStatus();   break;
        case DisplayPage::LOCATION: _drawLocation(); break;
        case DisplayPage::STATS:    _drawStats();    break;
        default:                    _drawStatus();   break;
    }
    _u8g2.sendBuffer();
}

// ---------------------------------------------------------------------------

void DisplayManager::setPage(DisplayPage page) {
    _currentPage    = page;
    _lastPageSwitch = millis();
}

// ---------------------------------------------------------------------------

void DisplayManager::sleep() {
    _u8g2.setPowerSave(1);
    _sleeping = true;
}

void DisplayManager::wake() {
    _u8g2.setPowerSave(0);
    _sleeping = false;
    _lastPageSwitch = millis();
}

// ---------------------------------------------------------------------------
// Page renderers
// ---------------------------------------------------------------------------

void DisplayManager::_drawStatus() {
    _u8g2.setFont(u8g2_font_6x10_tf);

    // Header bar
    _u8g2.drawStr(0, 10, DEVICE_ID);
    _drawBatteryIcon(100, 1, _battSnap.percent, _battSnap.charging);
    _drawSignalBars(116, 1, _modemSnap.rssi_dbm);

    _u8g2.drawHLine(0, 13, 128);

    // Fix status
    char buf[32];
    if (_gpsSnap.valid) {
        snprintf(buf, sizeof(buf), "FIX  %u sats  HDOP %.1f",
                 _gpsSnap.satellites, _gpsSnap.hdop);
    } else {
        snprintf(buf, sizeof(buf), "SEARCHING...");
    }
    _u8g2.drawStr(0, 26, buf);

    // Speed
    snprintf(buf, sizeof(buf), "Speed: %.1f km/h", _gpsSnap.speed_kmh);
    _u8g2.drawStr(0, 38, buf);

    // Battery
    snprintf(buf, sizeof(buf), "Batt: %u%%  %.2fV",
             _battSnap.percent, _battSnap.voltage_v);
    _u8g2.drawStr(0, 50, buf);

    // Queue depth
    snprintf(buf, sizeof(buf), "Queue: %u", _modemSnap.pending_records);
    _u8g2.drawStr(0, 62, buf);
}

// ---------------------------------------------------------------------------

void DisplayManager::_drawLocation() {
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.drawStr(0, 10, "LOCATION");
    _u8g2.drawHLine(0, 13, 128);

    char buf[32];
    snprintf(buf, sizeof(buf), "Lat  %+.6f", _gpsSnap.latitude);
    _u8g2.drawStr(0, 26, buf);
    snprintf(buf, sizeof(buf), "Lon  %+.6f", _gpsSnap.longitude);
    _u8g2.drawStr(0, 38, buf);
    snprintf(buf, sizeof(buf), "Alt  %.1f m", _gpsSnap.altitude_m);
    _u8g2.drawStr(0, 50, buf);
    snprintf(buf, sizeof(buf), "Hdg  %.0f deg", _gpsSnap.heading_deg);
    _u8g2.drawStr(0, 62, buf);
}

// ---------------------------------------------------------------------------

void DisplayManager::_drawStats() {
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.drawStr(0, 10, "STATS");
    _u8g2.drawHLine(0, 13, 128);

    char buf[32];
    snprintf(buf, sizeof(buf), "Uptime: %lus", millis() / 1000UL);
    _u8g2.drawStr(0, 26, buf);

    const char* regStr = _modemSnap.registered ? "Online" : "Offline";
    snprintf(buf, sizeof(buf), "LTE: %s  %d dBm", regStr, _modemSnap.rssi_dbm);
    _u8g2.drawStr(0, 38, buf);

    snprintf(buf, sizeof(buf), "Pending TX: %u", _modemSnap.pending_records);
    _u8g2.drawStr(0, 50, buf);

    snprintf(buf, sizeof(buf), "FW: " FIRMWARE_VERSION);
    _u8g2.drawStr(0, 62, buf);
}

// ---------------------------------------------------------------------------
// Widget helpers
// ---------------------------------------------------------------------------

void DisplayManager::_drawBatteryIcon(uint8_t x, uint8_t y, uint8_t pct, bool charging) {
    // Outer rectangle 14×8, nub 2×4 on right
    _u8g2.drawFrame(x, y, 14, 8);
    _u8g2.drawBox(x + 14, y + 2, 2, 4);

    uint8_t fill = static_cast<uint8_t>((pct * 12) / 100);
    if (fill > 0) _u8g2.drawBox(x + 1, y + 1, fill, 6);

    if (charging) {
        _u8g2.setFont(u8g2_font_4x6_tf);
        _u8g2.drawStr(x + 3, y + 7, "+");
        _u8g2.setFont(u8g2_font_6x10_tf);
    }
}

void DisplayManager::_drawSignalBars(uint8_t x, uint8_t y, int8_t rssi) {
    // 4 bars, each 3 px wide; height increases left to right
    // rssi 0 = unknown, -51..-70 = good, -71..-85 = fair, -86..-100 = poor
    uint8_t bars = 0;
    if      (rssi != 0 && rssi >= -70) bars = 4;
    else if (rssi >= -85)              bars = 3;
    else if (rssi >= -100)             bars = 2;
    else if (rssi != 0)                bars = 1;

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t h = (i + 1) * 2;
        uint8_t bx = x + i * 3;
        uint8_t by = y + 8 - h;
        if (i < bars) {
            _u8g2.drawBox(bx, by, 2, h);
        } else {
            _u8g2.drawFrame(bx, by, 2, h);
        }
    }
}
