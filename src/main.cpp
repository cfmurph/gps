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
// Timing state
// millis()-safe comparison: (uint32_t)(now - last) >= interval handles the
// ~49.7-day wrap correctly because unsigned subtraction wraps in the same way.
// ---------------------------------------------------------------------------
static uint32_t lastCaptureMs = 0;

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

    // Hardware watchdog — resets the device if loop() stalls longer than
    // WATCHDOG_TIMEOUT_MS (e.g. blocked modem AT command, infinite retry loop).
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms   = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic  = true,
    };
    esp_task_wdt_reconfigure(&wdt_cfg);
    esp_task_wdt_add(nullptr);  // Subscribe the current (main) task

    initHardware();
}

// ---------------------------------------------------------------------------

void loop() {
    // Pat the watchdog at the start of every loop so blocking sections
    // (modem, SD) don't inadvertently trigger a reset mid-operation.
    esp_task_wdt_reset();

    // -----------------------------------------------------------------------
    // 1. Battery sample + power mode evaluation
    // -----------------------------------------------------------------------
    BatteryStatus batt = battery.sample();
    psm.evaluate(batt, gps.latestRecord());

    const PowerMode pmode = psm.mode();

    // -----------------------------------------------------------------------
    // 2. GPS: always-on poll (NORMAL) or duty-cycle window (power-save modes)
    // -----------------------------------------------------------------------
    const uint32_t captureInterval = psm.gpsIntervalMs();

    if (static_cast<uint32_t>(millis() - lastCaptureMs) >= captureInterval) {
        if (pmode == PowerMode::NORMAL) {
            // GPS module is always powered — just read whatever arrived
            gps.poll();
            captureRecord();
        } else {
            // Duty-cycle: power GPS on, block for a fix window, power off
            // Then light-sleep for the remainder of the capture interval.
            const uint32_t windowMs = psm.gpsFixWindowMs();
            bool gotFix = gps.acquireFix(windowMs);
            if (gotFix) captureRecord();

            // Sleep for the interval minus the time already spent in acquireFix
            const uint32_t elapsed = static_cast<uint32_t>(millis() - lastCaptureMs);
            if (captureInterval > elapsed) {
                psm.doLightSleep(captureInterval - elapsed);
            }
        }
        lastCaptureMs = millis();
    } else if (pmode == PowerMode::NORMAL) {
        // In normal mode, poll GPS every loop iteration so the FIFO never overflows
        gps.poll();
    }

    // -----------------------------------------------------------------------
    // 3. Upload / retry — gated by batch policy
    // -----------------------------------------------------------------------
    serviceUplink();

    // -----------------------------------------------------------------------
    // 4. Display — suppressed in power-save modes
    // -----------------------------------------------------------------------
    if (psm.isDisplaySuppressed()) {
        display.sleep();
    } else {
        ModemStatus modemSnap;
        lte.fillStatus(modemSnap, queue.size());
        display.updateBattery(batt);
        display.updateModem(modemSnap);
        display.refresh();
        display.wake();
    }

    // -----------------------------------------------------------------------
    // 5. Yield — in power-save modes the light sleep above already yielded
    //    the CPU; only delay in NORMAL mode to avoid busy-spinning.
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

    queue.begin();

    // Initialise power-save manager from the first battery reading
    psm.begin(battery.status());

    // LTE init is slow — attempt asynchronously and continue if it fails;
    // RetryManager will re-establish the bearer before the first POST.
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

    // Attach battery level to the record before storing/transmitting
    rec.battery_pct = battery.status().percent;

    // Sync system time from GPS on the first valid fix
    if (!TimeUtils::isSynced() && rec.timestamp > 0) {
        TimeUtils::setTime(rec.timestamp);
    }

    // Log to SD
    sdLogger.log(rec);

    // Enqueue for LTE upload
    queue.push(rec);

    // Keep display GPS snapshot fresh
    display.updateGps(rec);
}

// ---------------------------------------------------------------------------

static void serviceUplink() {
    if (!psm.shouldUploadNow(queue.size())) return;

    // Drain as many queued records as possible in one pass
    bool delivered = retry.tick();

    // In power-save modes, disconnect the bearer after the batch is drained
    // to eliminate modem idle current between upload windows.
    if (delivered
            && psm.mode() != PowerMode::NORMAL
            && queue.empty()) {
        lte.disconnectBearer();
    }
}
