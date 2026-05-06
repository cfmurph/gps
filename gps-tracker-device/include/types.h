#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// Core data structures shared across all modules
// ---------------------------------------------------------------------------

/** A single validated GPS fix bundled with metadata ready for storage/upload. */
struct GPSRecord {
    uint32_t timestamp;    ///< Unix epoch (UTC), sourced from GPS or NTP fallback
    double   latitude;     ///< Decimal degrees, WGS-84, positive = North
    double   longitude;    ///< Decimal degrees, WGS-84, positive = East
    float    altitude_m;   ///< Metres above mean sea level
    float    speed_kmh;    ///< Ground speed
    float    heading_deg;  ///< True course over ground [0, 360)
    float    hdop;         ///< Horizontal dilution of precision (lower = better)
    uint8_t  satellites;   ///< Number of satellites used in fix
    uint8_t  battery_pct;  ///< Battery state-of-charge [0, 100]
    bool     valid;        ///< True when all mandatory fields are populated
};

/** Battery state reported by BatteryManager. */
struct BatteryStatus {
    float   voltage_v;    ///< Raw filtered cell voltage
    uint8_t percent;      ///< Estimated state-of-charge [0, 100]
    bool    charging;     ///< True when charge IC CHRG pin is asserted
    bool    low;          ///< True when percent falls below LOW_BATTERY_THRESHOLD
};

/** Connectivity / uplink health reported by LteManager. */
struct ModemStatus {
    bool    registered;      ///< Attached to a GPRS/LTE bearer
    int8_t  rssi_dbm;        ///< Signal strength (negative; 0 = unknown)
    uint8_t pending_records; ///< Records waiting in QueueManager
};

/** Display page selector used by DisplayManager. */
enum class DisplayPage : uint8_t {
    STATUS  = 0,  ///< Device health overview (battery, signal, fix age)
    LOCATION,     ///< Lat/lon/alt/speed
    STATS,        ///< Upload counters, SD space, uptime
    PAGE_COUNT    ///< Sentinel — keep last
};

/** Retry outcome returned by RetryManager::attempt(). */
enum class RetryResult : uint8_t {
    SUCCESS,        ///< Record delivered, remove from queue
    TRANSIENT_FAIL, ///< Network hiccup — keep in queue, back off
    PERMANENT_FAIL, ///< Malformed payload etc. — discard record
};
