#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Device identity
// ---------------------------------------------------------------------------
#define DEVICE_ID          "TRACKER-001"
#define FIRMWARE_VERSION   "1.0.0"

// ---------------------------------------------------------------------------
// Serial baud rates
// ---------------------------------------------------------------------------
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200;
constexpr uint32_t GPS_BAUD          = 9600;
constexpr uint32_t LTE_BAUD          = 115200;

// ---------------------------------------------------------------------------
// GPS
// ---------------------------------------------------------------------------
constexpr uint32_t GPS_FIX_TIMEOUT_MS       = 60000;  ///< Max wait for first fix
constexpr uint32_t GPS_RECORD_INTERVAL_MS   = 5000;   ///< How often to capture a fix
constexpr uint8_t  GPS_MIN_SATELLITES       = 4;      ///< Minimum sats for valid fix
constexpr float    GPS_MAX_HDOP             = 3.0f;   ///< Discard fixes worse than this
constexpr float    GPS_MIN_SPEED_FILTER_KMH = 0.5f;   ///< Zero-velocity noise floor

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
constexpr uint8_t  OLED_I2C_ADDR          = 0x3C;
constexpr uint32_t DISPLAY_PAGE_CYCLE_MS  = 5000;    ///< Auto-advance interval
constexpr uint8_t  DISPLAY_CONTRAST       = 200;

// ---------------------------------------------------------------------------
// SD card / logging
// ---------------------------------------------------------------------------
constexpr uint32_t SD_FLUSH_INTERVAL_MS   = 10000;   ///< Force fsync every N ms
constexpr uint32_t SD_ROTATE_BYTES        = 5242880; ///< Rotate CSV at 5 MB
constexpr char     SD_LOG_DIR[]           = "/logs";

// ---------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------
constexpr float   BATT_DIVIDER_RATIO     = 2.0f;   ///< R1=R2=100 kΩ — adjust if different
constexpr float   BATT_ADC_REF_V         = 3.3f;
constexpr float   BATT_FULL_V            = 4.2f;
constexpr float   BATT_EMPTY_V           = 3.0f;
constexpr uint8_t LOW_BATTERY_THRESHOLD  = 15;     ///< % below which LOW flag is set
constexpr float   BATT_IIR_ALPHA         = 0.1f;   ///< Low-pass filter coefficient

// ---------------------------------------------------------------------------
// LTE / uplink
// ---------------------------------------------------------------------------
// APN — override for your SIM provider
#define LTE_APN                "internet"
#define LTE_USER               ""
#define LTE_PASS               ""

// Server endpoint — store real values in a git-ignored secrets.h.
// SERVER_HOST must be a bare hostname or IP (no scheme, no path).
// SERVER_PATH must begin with '/'.
#ifndef SERVER_HOST
#  define SERVER_HOST          "example.com"
#endif
#ifndef SERVER_PATH
#  define SERVER_PATH          "/api/v1/track"
#endif
#ifndef SERVER_PORT
#  define SERVER_PORT          443
#endif

constexpr uint32_t LTE_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t HTTP_TIMEOUT_MS        = 15000;

// ---------------------------------------------------------------------------
// Queue / retry
// ---------------------------------------------------------------------------
constexpr uint8_t  QUEUE_MAX_RECORDS      = 120;    ///< Ring-buffer capacity (PSRAM)
constexpr uint8_t  RETRY_MAX_ATTEMPTS     = 5;
constexpr uint32_t RETRY_BASE_DELAY_MS    = 2000;   ///< Doubles each attempt (backoff)
constexpr uint32_t RETRY_MAX_DELAY_MS     = 60000;

// ---------------------------------------------------------------------------
// Task / timing
// ---------------------------------------------------------------------------
constexpr uint32_t WATCHDOG_TIMEOUT_MS    = 30000;
constexpr uint32_t LOOP_YIELD_MS          = 10;

// ---------------------------------------------------------------------------
// Power saving mode
// ---------------------------------------------------------------------------

// Battery % thresholds at which the mode transitions downward.
// Hysteresis of PS_HYSTERESIS_PCT prevents rapid toggling near a boundary.
constexpr uint8_t  PS_SAVING_THRESHOLD_PCT   = 30;  ///< NORMAL  → SAVING
constexpr uint8_t  PS_CRITICAL_THRESHOLD_PCT = 15;  ///< SAVING  → CRITICAL  (= LOW_BATTERY_THRESHOLD)
constexpr uint8_t  PS_HYSTERESIS_PCT         = 3;   ///< extra % required to step back up

// GPS duty-cycling: module is powered off between fix windows to save ~20 mA.
// The acquisition window must be long enough for a warm-start TTFF (~2–5 s).
constexpr uint32_t PS_GPS_INTERVAL_SAVING_MS   = 30000;  ///< capture every 30 s in SAVING
constexpr uint32_t PS_GPS_INTERVAL_CRITICAL_MS = 60000;  ///< capture every 60 s in CRITICAL
constexpr uint32_t PS_GPS_FIX_WINDOW_MS        = 15000;  ///< max time to wait for a fix

// Stationary detection: if speed stays below this for this many consecutive
// records, double the capture interval further (up to PS_GPS_INTERVAL_CRITICAL_MS).
constexpr float    PS_STATIONARY_SPEED_KMH  = 1.0f;
constexpr uint8_t  PS_STATIONARY_COUNT      = 3;

// LTE batching: in power-save modes, accumulate this many records before
// bringing up the bearer, draining the queue, then disconnecting.
// In NORMAL mode the bearer stays up and records are uploaded immediately.
constexpr uint8_t  PS_UPLOAD_BATCH_SAVING   = 6;
constexpr uint8_t  PS_UPLOAD_BATCH_CRITICAL = 12;

// Light sleep: ESP32 suspends between GPS capture windows.
// The CPU is halted (~0.8 mA vs ~240 mA active) while RTC and UART
// peripheral clocks remain on so the GPS UART can wake the chip early.
constexpr bool     PS_LIGHT_SLEEP_ENABLED   = true;
