#include "power_save_manager.h"
#include "../../include/config.h"
#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <driver/uart.h>

// ---------------------------------------------------------------------------

void PowerSaveManager::begin(const BatteryStatus& batt) {
    // Determine starting mode from initial battery reading
    if (batt.percent < PS_CRITICAL_THRESHOLD_PCT) {
        _mode = PowerMode::CRITICAL;
    } else if (batt.percent < PS_SAVING_THRESHOLD_PCT) {
        _mode = PowerMode::SAVING;
    } else {
        _mode = PowerMode::NORMAL;
    }
    Serial.printf("[PSM] Initial mode: %s  (batt %u%%)\n",
                  modeName(_mode), batt.percent);
}

// ---------------------------------------------------------------------------

void PowerSaveManager::evaluate(const BatteryStatus& batt,
                                 const GPSRecord& lastRecord) {
    // Track stationary state for adaptive interval extension
    if (lastRecord.valid && lastRecord.speed_kmh < PS_STATIONARY_SPEED_KMH) {
        if (_stationaryCount < UINT8_MAX) _stationaryCount++;
    } else {
        _stationaryCount = 0;
    }

    // Mode transitions with hysteresis to prevent oscillation
    const uint8_t pct = batt.percent;

    switch (_mode) {
        case PowerMode::NORMAL:
            if (pct < PS_SAVING_THRESHOLD_PCT) {
                _transitionTo(PowerMode::SAVING);
            }
            break;

        case PowerMode::SAVING:
            if (pct < PS_CRITICAL_THRESHOLD_PCT) {
                _transitionTo(PowerMode::CRITICAL);
            } else if (pct >= PS_SAVING_THRESHOLD_PCT + PS_HYSTERESIS_PCT) {
                _transitionTo(PowerMode::NORMAL);
            }
            break;

        case PowerMode::CRITICAL:
            if (pct >= PS_CRITICAL_THRESHOLD_PCT + PS_HYSTERESIS_PCT) {
                _transitionTo(PowerMode::SAVING);
            }
            break;
    }
}

// ---------------------------------------------------------------------------

uint32_t PowerSaveManager::gpsIntervalMs() const {
    switch (_mode) {
        case PowerMode::NORMAL:
            return GPS_RECORD_INTERVAL_MS;

        case PowerMode::SAVING: {
            // Double the interval when the device has been stationary for a while
            uint32_t base = PS_GPS_INTERVAL_SAVING_MS;
            if (_stationaryCount >= PS_STATIONARY_COUNT) base *= 2;
            return min(base, PS_GPS_INTERVAL_CRITICAL_MS);
        }

        case PowerMode::CRITICAL:
            return PS_GPS_INTERVAL_CRITICAL_MS;
    }
    return GPS_RECORD_INTERVAL_MS;  // unreachable
}

// ---------------------------------------------------------------------------

uint32_t PowerSaveManager::gpsFixWindowMs() const {
    // In NORMAL mode the GPS is always on — no concept of a fix window.
    // In power-save modes we allow PS_GPS_FIX_WINDOW_MS for a warm-start fix.
    return (_mode == PowerMode::NORMAL) ? 0 : PS_GPS_FIX_WINDOW_MS;
}

// ---------------------------------------------------------------------------

bool PowerSaveManager::shouldUploadNow(uint8_t queueDepth) const {
    if (_mode == PowerMode::NORMAL) return true;  // always upload
    return queueDepth >= uploadBatchSize();
}

// ---------------------------------------------------------------------------

uint8_t PowerSaveManager::uploadBatchSize() const {
    switch (_mode) {
        case PowerMode::NORMAL:   return 1;
        case PowerMode::SAVING:   return PS_UPLOAD_BATCH_SAVING;
        case PowerMode::CRITICAL: return PS_UPLOAD_BATCH_CRITICAL;
    }
    return 1;
}

// ---------------------------------------------------------------------------

bool PowerSaveManager::isDisplaySuppressed() const {
    return _mode != PowerMode::NORMAL;
}

// ---------------------------------------------------------------------------

void PowerSaveManager::doLightSleep(uint32_t ms) {
    if (!PS_LIGHT_SLEEP_ENABLED)           return;
    if (_mode == PowerMode::NORMAL)        return;
    if (ms < 100)                          return;  // not worth the overhead

    esp_task_wdt_reset();

    // Configure timer wakeup
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(ms) * 1000ULL);

    // Configure UART2 (GPS) as a wakeup source so an incoming NMEA byte
    // wakes the CPU before the timer fires — useful once GPS is powered back
    // on before the sleep ends.
    esp_sleep_enable_uart_wakeup(UART_NUM_2);

    Serial.printf("[PSM] Light sleep %lu ms\n", static_cast<unsigned long>(ms));
    Serial.flush();

    esp_light_sleep_start();

    // CPU is back — clear wakeup config for the next sleep
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

    esp_task_wdt_reset();
}

// ---------------------------------------------------------------------------

const char* PowerSaveManager::modeName(PowerMode m) {
    switch (m) {
        case PowerMode::NORMAL:   return "NORMAL";
        case PowerMode::SAVING:   return "SAVING";
        case PowerMode::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

// ---------------------------------------------------------------------------

void PowerSaveManager::_transitionTo(PowerMode next) {
    if (next == _mode) return;
    Serial.printf("[PSM] %s → %s\n", modeName(_mode), modeName(next));
    _mode = next;
}
