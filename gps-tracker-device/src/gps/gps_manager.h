#pragma once

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// GpsManager
//
// Owns UART2 and the TinyGPS++ parser.  Call poll() inside the main loop;
// when a valid fix is ready, latestRecord() returns a populated GPSRecord.
// ---------------------------------------------------------------------------
class GpsManager {
public:
    GpsManager() = default;

    /** Initialise UART2 and assert GPS module power-enable pin. */
    void begin();

    /**
     * Feed all waiting bytes into the NMEA parser.
     * Must be called every loop iteration to avoid overrunning the UART FIFO.
     * Returns true if at least one new valid fix was processed this call.
     */
    bool poll();

    /** True once a qualifying fix has been received (sats ≥ min, HDOP ≤ max). */
    bool hasFix() const { return _hasFix; }

    /**
     * Returns the most recently validated record.
     * The caller should check GPSRecord::valid before using the data.
     */
    const GPSRecord& latestRecord() const { return _record; }

    /** Milliseconds since the last valid fix was captured (for staleness checks). */
    uint32_t msSinceLastFix() const;

    /** Power-cycle the GPS module (useful after extended signal loss). */
    void reset();

private:
    TinyGPSPlus _gps;
    GPSRecord   _record{};
    bool        _hasFix        = false;
    uint32_t    _lastFixMs     = 0;
    uint32_t    _lastPollMs    = 0;

    /** Populate _record from the current TinyGPS+ objects. */
    bool _buildRecord();
};
