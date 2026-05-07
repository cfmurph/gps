#include <Arduino.h>
#include <esp_task_wdt.h>

#include "../include/config.h"
#include "gps/gps_manager.h"
#include "display/display_manager.h"
#include "storage/sd_logger.h"
#include "power/battery_manager.h"
#include "power/power_save_manager.h"
#include "comms/lte_manager.h"
#include "comms/queue_manager.h"
#include "comms/retry_manager.h"
#include "utils/time_utils.h"

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
static GpsManager       gps;
static DisplayManager   display;
static SdLogger         sdLogger;
static BatteryManager   battery;
static PowerSaveManager psm;
static LteManager       lte;
static QueueManager     queue;
static RetryManager     retry(lte, queue);

// ---------------------------------------------------------------------------
// Per-subsystem timing state
// All millis() comparisons use unsigned subtraction to survive the ~49.7-day
// counter rollover: (uint32_t)(now - last) >= interval.
// ---------------------------------------------------------------------------
static uint32_t lastCaptureMs     = 0;
static uint32_t lastBattSampleMs  = 0;
static uint32_t lastDisplayMs     = 0;
static uint32_t lastModemStatusMs = 0;
static uint32_t lastPostMs        = 0;

// Latest snapshots refreshed at their own rates
static BatteryStatus lastBatt{};
static ModemStatus   lastModem{};

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void initHardware();
static void captureRecord();
static void serviceUplink();

// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(SERIAL_DEBUG_BAUD);
    delay(500);
    Serial.println(F("\n== GPS Tracker " FIRMWARE_VERSION " =="));

    // Force TZ to UTC so mktime/gmtime produce consistent results
    setenv("TZ", "UTC0", 1);
    tzset();

    // initHardware() contains slow blocking operations (modem init, ~30 s).
    // Subscribe to the watchdog AFTER it returns so those operations don't
    // consume the entire watchdog budget before loop() ever runs.
    initHardware();

    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms    = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&wdt_cfg);
    esp_task_wdt_add(nullptr);
}

// ---------------------------------------------------------------------------

void loop() {
    esp_task_wdt_reset();

    const uint32_t now = millis();

    // -----------------------------------------------------------------------
    // 1. Battery — sampled at BATTERY_SAMPLE_INTERVAL_MS (2 Hz)
    //    Avoids 8 ADC reads + IIR every iteration (~every 10 ms).
    // -----------------------------------------------------------------------
    if (static_cast<uint32_t>(now - lastBattSampleMs) >= BATTERY_SAMPLE_INTERVAL_MS) {
        lastBatt         = battery.sample();
        lastBattSampleMs = now;
        psm.evaluate(lastBatt, gps.latestRecord());
    }

    const PowerMode pmode = psm.mode();

    // -----------------------------------------------------------------------
    // 2. GPS — always-on poll (NORMAL) or duty-cycle window (power-save)
    // -----------------------------------------------------------------------
    const uint32_t captureInterval = psm.gpsIntervalMs();

    if (static_cast<uint32_t>(now - lastCaptureMs) >= captureInterval) {
        if (pmode == PowerMode::NORMAL) {
            gps.poll();
            captureRecord();
        } else {
            const uint32_t windowMs = psm.gpsFixWindowMs();
            bool gotFix = gps.acquireFix(windowMs);
            if (gotFix) captureRecord();

            // Light-sleep for the remaining interval
            const uint32_t elapsed = static_cast<uint32_t>(millis() - lastCaptureMs);
            if (captureInterval > elapsed) {
                psm.doLightSleep(captureInterval - elapsed);
            }
        }
        lastCaptureMs = millis();
    } else if (pmode == PowerMode::NORMAL) {
        // In NORMAL mode, drain GPS UART every iteration so the enlarged 4096-
        // byte buffer never fills between capture windows.
        gps.poll();
    }

    // -----------------------------------------------------------------------
    // 3. Upload — gated by batch policy and minimum inter-POST interval
    // -----------------------------------------------------------------------
    if (static_cast<uint32_t>(now - lastPostMs) >= MIN_POST_INTERVAL_MS) {
        if (serviceUplink()) {
            lastPostMs = millis();
        }
    }

    // -----------------------------------------------------------------------
    // 4. Modem status — queried at MODEM_STATUS_INTERVAL_MS (1 Hz)
    //    Each call issues AT commands over UART; caching avoids stalling
    //    the loop on every iteration.
    // -----------------------------------------------------------------------
    if (!psm.isDisplaySuppressed()
            && static_cast<uint32_t>(now - lastModemStatusMs) >= MODEM_STATUS_INTERVAL_MS) {
        lte.fillStatus(lastModem, queue.size());
        lastModemStatusMs = now;
    }

    // -----------------------------------------------------------------------
    // 5. Display — refreshed at DISPLAY_REFRESH_INTERVAL_MS (4 Hz)
    //    sendBuffer() is ~20 ms of synchronous I2C; calling it every loop
    //    (~10 ms yield) would pin ~66% of loop time in I2C transfers.
    // -----------------------------------------------------------------------
    if (psm.isDisplaySuppressed()) {
        display.sleep();
    } else if (static_cast<uint32_t>(now - lastDisplayMs) >= DISPLAY_REFRESH_INTERVAL_MS) {
        display.updateBattery(lastBatt);
        display.updateModem(lastModem);
        display.refresh();
        display.wake();
        lastDisplayMs = now;
    }

    // -----------------------------------------------------------------------
    // 6. Yield — in power-save modes the light sleep already yielded the CPU.
    // -----------------------------------------------------------------------
    if (pmode == PowerMode::NORMAL) {
        delay(LOOP_YIELD_MS);
    }
}

// ---------------------------------------------------------------------------

static void initHardware() {
    display.begin();
    gps.begin();

    bool sdOk = sdLogger.begin();
    if (!sdOk) {
        Serial.println(F("[MAIN] SD unavailable — records will queue in RAM only"));
    }

    battery.begin();
    lastBatt = battery.status();

    queue.begin();
    psm.begin(lastBatt);

    Serial.println(F("[MAIN] Initialising LTE modem (may take ~30 s)..."));
    if (!lte.begin()) {
        Serial.println(F("[MAIN] LTE init failed — will retry on first upload attempt"));
    }

    Serial.println(F("[MAIN] Hardware init complete"));
}

// ---------------------------------------------------------------------------

static void captureRecord() {
    if (!gps.hasFix()) return;

    GPSRecord rec = gps.latestRecord();
    if (!rec.valid) return;

    rec.battery_pct = lastBatt.percent;

    if (!TimeUtils::isSynced() && rec.timestamp > 0) {
        TimeUtils::setTime(rec.timestamp);
    }

    sdLogger.log(rec);
    queue.push(rec);
    display.updateGps(rec);
}

// ---------------------------------------------------------------------------

/** Attempt one upload from the queue. Returns true if a record was delivered. */
static bool serviceUplink() {
    if (!psm.shouldUploadNow(queue.size())) return false;

    bool delivered = retry.tick();

    // In power-save modes, drop the bearer after the queue is empty so the
    // modem does not draw idle current between batch upload windows.
    if (delivered && psm.mode() != PowerMode::NORMAL && queue.empty()) {
        lte.disconnectBearer();
    }

    return delivered;
}
