#include "time_utils.h"
#include <time.h>
#include <sys/time.h>

static bool s_synced = false;

// ---------------------------------------------------------------------------

void TimeUtils::epochToString(uint32_t epoch, char* buf, size_t len) {
    struct tm t{};
    time_t e = static_cast<time_t>(epoch);
    gmtime_r(&e, &t);
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
}

// ---------------------------------------------------------------------------

uint32_t TimeUtils::parseCclk(const char* cclk) {
    // Expected: "yy/MM/dd,hh:mm:ss±zz"  (surrounding quotes may be present)
    if (!cclk) return 0;

    // Width limits (%2d, %1c, %2d) prevent int overflow from oversized fields.
    // sign and tz are initialised so that matched==7 (no tz field) is safe.
    int  yy = 0, mo = 0, dd = 0, hh = 0, mm = 0, ss = 0, tz = 0;
    char sign = '+';
    int matched = sscanf(cclk, "\"%2d/%2d/%2d,%2d:%2d:%2d%1c%2d\"",
                         &yy, &mo, &dd, &hh, &mm, &ss, &sign, &tz);
    // Require at least the 6 date/time fields; tz offset is optional
    if (matched < 6) return 0;

    // Sanity-check calendar fields before feeding to mktime
    if (mo < 1 || mo > 12 || dd < 1 || dd > 31) return 0;
    if (hh > 23 || mm > 59 || ss > 60)           return 0;

    struct tm t{};
    t.tm_year = yy + 100;  // 2-digit year offset from 1900
    t.tm_mon  = mo - 1;
    t.tm_mday = dd;
    t.tm_hour = hh;
    t.tm_min  = mm;
    t.tm_sec  = ss;

    time_t epoch = mktime(&t);  // NB: mktime uses local time; ensure TZ=UTC
    if (epoch == (time_t)-1) return 0;

    // Adjust for UTC offset only when the tz field was actually parsed
    if (matched == 8) {
        // tz is in quarter-hours; clamp to ±56 (±14 h) to prevent overflow
        if (tz < 0 || tz > 56) tz = 0;
        long offsetSec = static_cast<long>(tz) * 15L * 60L;
        if (sign == '-') epoch += static_cast<time_t>(offsetSec);
        else             epoch -= static_cast<time_t>(offsetSec);
    }

    if (epoch < 0) return 0;
    return static_cast<uint32_t>(epoch);
}

// ---------------------------------------------------------------------------

uint32_t TimeUtils::now() {
    if (!s_synced) return 0;
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return static_cast<uint32_t>(tv.tv_sec);
}

// ---------------------------------------------------------------------------

void TimeUtils::setTime(uint32_t epoch) {
    struct timeval tv{ static_cast<time_t>(epoch), 0 };
    settimeofday(&tv, nullptr);
    s_synced = true;
    Serial.printf("[TIME] RTC synchronised to epoch %lu\n",
                  static_cast<unsigned long>(epoch));
}

// ---------------------------------------------------------------------------

bool TimeUtils::isSynced() {
    return s_synced;
}

// ---------------------------------------------------------------------------

void TimeUtils::formatUptime(uint32_t ms, char* buf, size_t len) {
    uint32_t totalSec = ms / 1000;
    uint32_t h = totalSec / 3600;
    uint32_t m = (totalSec % 3600) / 60;
    uint32_t s = totalSec % 60;
    snprintf(buf, len, "%02lu:%02lu:%02lu",
             static_cast<unsigned long>(h),
             static_cast<unsigned long>(m),
             static_cast<unsigned long>(s));
}
