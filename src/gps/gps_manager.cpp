#include "gps_manager.h"
#include "../../include/config.h"
#include "../../include/pins.h"

// UART2 is the hardware serial port wired to the GPS module.
// Using HardwareSerial directly avoids SoftwareSerial timing issues at 9600 baud.
static HardwareSerial GpsSerial(2);

// ---------------------------------------------------------------------------

void GpsManager::begin() {
    pinMode(PIN_GPS_EN, OUTPUT);
    digitalWrite(PIN_GPS_EN, HIGH);  // Power up the module

    GpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

    Serial.println(F("[GPS] UART2 initialised"));
}

// ---------------------------------------------------------------------------

bool GpsManager::poll() {
    bool gotNewFix = false;

    // Cap bytes consumed per call so a flooded/noisy UART cannot stall the
    // main loop indefinitely.  At 9600 baud, 256 bytes takes ~27 ms — well
    // within one loop iteration budget.  Any remaining bytes are drained on
    // the next call.
    constexpr int kMaxBytesPerPoll = 256;
    int budget = kMaxBytesPerPoll;

    while (budget-- > 0 && GpsSerial.available() > 0) {
        char c = static_cast<char>(GpsSerial.read());
        if (_gps.encode(c)) {
            // TinyGPS+ has finished parsing a full NMEA sentence
            if (_gps.location.isUpdated() && _gps.date.isValid() && _gps.time.isValid()) {
                if (_buildRecord()) {
                    _hasFix    = true;
                    _lastFixMs = millis();
                    gotNewFix  = true;
                }
            }
        }
    }

    // Warn every 30 s if no characters arrive at all (module power issue?)
    if (static_cast<uint32_t>(millis() - _lastPollMs) > 30000
            && _gps.charsProcessed() < 10) {
        Serial.println(F("[GPS] Warning: no NMEA data received — check wiring"));
    }
    _lastPollMs = millis();

    return gotNewFix;
}

// ---------------------------------------------------------------------------

uint32_t GpsManager::msSinceLastFix() const {
    if (_lastFixMs == 0) return UINT32_MAX;
    return millis() - _lastFixMs;
}

// ---------------------------------------------------------------------------

bool GpsManager::acquireFix(uint32_t timeoutMs) {
    // Power on
    digitalWrite(PIN_GPS_EN, HIGH);
    GpsSerial.flush();

    Serial.printf("[GPS] Duty-cycle window: %lu ms\n",
                  static_cast<unsigned long>(timeoutMs));

    const uint32_t deadline = millis() + timeoutMs;
    bool gotFix = false;

    while (static_cast<uint32_t>(millis() - deadline) > 0x7FFFFFFFUL) {
        constexpr int kMaxBytesPerPoll = 256;
        int budget = kMaxBytesPerPoll;
        while (budget-- > 0 && GpsSerial.available() > 0) {
            char c = static_cast<char>(GpsSerial.read());
            if (_gps.encode(c)) {
                if (_gps.location.isUpdated()
                        && _gps.date.isValid()
                        && _gps.time.isValid()) {
                    if (_buildRecord()) {
                        _hasFix    = true;
                        _lastFixMs = millis();
                        gotFix     = true;
                    }
                }
            }
        }
        if (gotFix) break;
        delay(10);
    }

    // Power off regardless of outcome to save current
    digitalWrite(PIN_GPS_EN, LOW);

    Serial.printf("[GPS] Duty-cycle complete: %s\n", gotFix ? "fix" : "no fix");
    return gotFix;
}

// ---------------------------------------------------------------------------

void GpsManager::reset() {
    Serial.println(F("[GPS] Power-cycling module"));
    digitalWrite(PIN_GPS_EN, LOW);
    delay(500);
    digitalWrite(PIN_GPS_EN, HIGH);
    _hasFix = false;
    _record = GPSRecord{};
    _lastFixMs = 0;
}

// ---------------------------------------------------------------------------

bool GpsManager::_buildRecord() {
    // Reject fixes that don't meet quality thresholds
    if (_gps.satellites.value() < GPS_MIN_SATELLITES) return false;
    if (_gps.hdop.hdop()        > GPS_MAX_HDOP)       return false;
    if (!_gps.location.isValid())                      return false;

    // Derive a Unix timestamp from the GPS date/time fields.
    // TinyGPS+ provides individual fields; combine into epoch seconds.
    struct tm t{};
    t.tm_year = _gps.date.year()  - 1900;
    t.tm_mon  = _gps.date.month() - 1;
    t.tm_mday = _gps.date.day();
    t.tm_hour = _gps.time.hour();
    t.tm_min  = _gps.time.minute();
    t.tm_sec  = _gps.time.second();
    // mktime uses local time; TZ must be set to UTC (done in setup()).
    time_t epoch = mktime(&t);
    // mktime returns -1 on error; a negative epoch would corrupt the timestamp.
    if (epoch <= 0) return false;

    _record.timestamp   = static_cast<uint32_t>(epoch);
    _record.latitude    = _gps.location.lat();
    _record.longitude   = _gps.location.lng();
    _record.altitude_m  = static_cast<float>(_gps.altitude.meters());
    _record.speed_kmh   = static_cast<float>(_gps.speed.kmph());
    _record.heading_deg = static_cast<float>(_gps.course.deg());
    _record.hdop        = _gps.hdop.hdop();
    _record.satellites  = static_cast<uint8_t>(_gps.satellites.value());
    _record.valid       = true;

    // Clamp near-zero speed to zero to suppress vibration noise
    if (_record.speed_kmh < GPS_MIN_SPEED_FILTER_KMH) {
        _record.speed_kmh   = 0.0f;
        _record.heading_deg = 0.0f;
    }

    return true;
}
