#include <Arduino.h>
#include <esp_task_wdt.h>

#include "../include/config.h"
#include "gps/gps_manager.h"
#include "display/display_manager.h"
#include "storage/sd_logger.h"
#include "power/battery_manager.h"
#include "comms/lte_manager.h"
#include "comms/queue_manager.h"
#include "comms/retry_manager.h"
#include "utils/time_utils.h"

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
static GpsManager     gps;
static DisplayManager display;
static SdLogger       sdLogger;
static BatteryManager battery;
static LteManager     lte;
static QueueManager   queue;
static RetryManager   retry(lte, queue);

// ---------------------------------------------------------------------------
// Timing state
// millis()-safe comparison: (uint32_t)(now - last) >= interval handles the
// ~49.7-day wrap correctly because unsigned subtraction wraps in the same way.
// ---------------------------------------------------------------------------
static uint32_t lastRecordMs  = 0;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void initHardware();
static void captureAndEnqueue();
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

    // 1. Feed the GPS UART parser — must run every iteration
    gps.poll();

    // 2. Sample battery (cheap ADC read + IIR update)
    BatteryStatus batt = battery.sample();

    // 3. Capture a new GPS record on the configured interval.
    //    Cast to uint32_t makes the subtraction wrap-safe across millis() rollover.
    if (static_cast<uint32_t>(millis() - lastRecordMs) >= GPS_RECORD_INTERVAL_MS) {
        captureAndEnqueue();
        lastRecordMs = millis();
    }

    // 4. Drive the retry/delivery loop
    serviceUplink();

    // 5. Update the display snapshot and refresh (non-blocking)
    ModemStatus modemSnap;
    lte.fillStatus(modemSnap, queue.size());
    display.updateBattery(batt);
    display.updateModem(modemSnap);
    display.refresh();

    // 6. Sleep the display on low battery to conserve power
    if (batt.low) {
        display.sleep();
    }

    delay(LOOP_YIELD_MS);
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

    // LTE init is slow — attempt asynchronously and continue if it fails;
    // RetryManager will re-establish the bearer before the first POST.
    Serial.println(F("[MAIN] Initialising LTE modem (may take ~30 s)..."));
    if (!lte.begin()) {
        Serial.println(F("[MAIN] LTE init failed — will retry on first upload attempt"));
    }

    // Sync system clock from modem CCLK if LTE came up
    // (GPS time sync happens inside GpsManager once a fix is obtained)
    Serial.println(F("[MAIN] Hardware init complete"));
}

// ---------------------------------------------------------------------------

static void captureAndEnqueue() {
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

    if (queue.full()) {
        Serial.println(F("[MAIN] Queue full — oldest record dropped"));
    }
}

// ---------------------------------------------------------------------------

static void serviceUplink() {
    // Attempt to drain one record from the queue per loop pass.
    // RetryManager enforces backoff internally.
    retry.tick();
}
