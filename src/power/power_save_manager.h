#pragma once

#include <Arduino.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// PowerSaveManager
//
// Central policy object that observes battery SOC and motion state, decides
// which PowerMode the device should be in, and provides the current mode's
// operational parameters to the rest of the firmware.
//
// Mode transitions:
//
//   NORMAL  ──(batt < PS_SAVING_THRESHOLD_PCT)──►  SAVING
//   SAVING  ──(batt < PS_CRITICAL_THRESHOLD_PCT)──► CRITICAL
//   SAVING  ──(batt > SAVING_THRESHOLD + HYSTERESIS)──► NORMAL
//   CRITICAL──(batt > CRITICAL_THRESHOLD + HYSTERESIS)──► SAVING
//
// Main loop usage pattern:
//
//   psm.evaluate(battStatus, lastRecord);     // update state
//
//   if (psm.shouldCaptureNow()) {
//       gps.acquireFix(psm.gpsFixWindowMs()); // duty-cycled GPS window
//   }
//   if (psm.shouldUploadNow(queue.size())) {
//       retry.tick();
//       if (psm.mode() != PowerMode::NORMAL) lte.disconnectBearer();
//   }
//   if (psm.lightSleepDueMs() > 0) {
//       psm.doLightSleep();
//   }
// ---------------------------------------------------------------------------
class PowerSaveManager {
public:
    PowerSaveManager() = default;

    /** Seed the initial mode from the first battery reading. */
    void begin(const BatteryStatus& batt);

    /**
     * Re-evaluate the mode given the latest battery and GPS data.
     * Call once per capture cycle (not every loop iteration).
     */
    void evaluate(const BatteryStatus& batt, const GPSRecord& lastRecord);

    /** Current operating mode. */
    PowerMode mode() const { return _mode; }

    // -----------------------------------------------------------------------
    // Per-mode operational parameters
    // -----------------------------------------------------------------------

    /** How often the main loop should attempt a GPS capture. */
    uint32_t gpsIntervalMs() const;

    /** How long to keep the GPS module powered up waiting for a fix. */
    uint32_t gpsFixWindowMs() const;

    /**
     * True when at least batchSize() records are queued, or when the
     * mode is NORMAL (upload immediately).
     */
    bool shouldUploadNow(uint8_t queueDepth) const;

    /** Minimum records to accumulate before opening a bearer in power-save modes. */
    uint8_t uploadBatchSize() const;

    /** True when the display should be kept off to save power. */
    bool isDisplaySuppressed() const;

    /**
     * Enter ESP32 light sleep for up to `ms` milliseconds.
     * Configures UART0 (GPS) as a wakeup source so an incoming NMEA byte
     * wakes the CPU early. Pats the watchdog before and after.
     *
     * No-op when PS_LIGHT_SLEEP_ENABLED == false or mode == NORMAL.
     */
    void doLightSleep(uint32_t ms);

    /** Human-readable mode name for logging. */
    static const char* modeName(PowerMode m);

private:
    PowerMode _mode             = PowerMode::NORMAL;
    uint8_t   _stationaryCount  = 0;

    void _transitionTo(PowerMode next);
};
