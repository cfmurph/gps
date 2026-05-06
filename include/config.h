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

// Endpoint that accepts JSON POST — store real URL in secrets.h (git-ignored)
#ifndef SERVER_URL
#  define SERVER_URL           "https://example.com/api/v1/track"
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
