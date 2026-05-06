#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// TimeUtils
//
// Stateless helpers for time conversions and formatting.
// The ESP32 standard library provides mktime/gmtime_r; these wrappers add
// convenience formatting and NTP-from-modem functionality.
// ---------------------------------------------------------------------------
namespace TimeUtils {

    /**
     * Convert a Unix epoch (UTC) to a human-readable string.
     * Output format: "2026-05-06 14:32:00"
     * buf must be at least 20 bytes.
     */
    void epochToString(uint32_t epoch, char* buf, size_t len);

    /**
     * Parse a CCLK response from the SIM7600 into a Unix epoch.
     * CCLK format: "yy/MM/dd,hh:mm:ss±zz"
     * Returns 0 on parse error.
     */
    uint32_t parseCclk(const char* cclk);

    /**
     * Return the current Unix epoch from the system RTC (set via setTime).
     * Returns 0 if the RTC has never been synchronised.
     */
    uint32_t now();

    /**
     * Set the ESP32 POSIX clock from a Unix epoch value.
     * Should be called once after modem/NTP sync.
     */
    void setTime(uint32_t epoch);

    /** True if setTime has been called at least once since boot. */
    bool isSynced();

    /**
     * Format an elapsed millisecond count as "HH:MM:SS".
     * buf must be at least 9 bytes.
     */
    void formatUptime(uint32_t ms, char* buf, size_t len);

} // namespace TimeUtils
