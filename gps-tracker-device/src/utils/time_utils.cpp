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

    int yy, mo, dd, hh, mm, ss, tz;
    char sign;
    int matched = sscanf(cclk, "\"%d/%d/%d,%d:%d:%d%c%d\"",
                         &yy, &mo, &dd, &hh, &mm, &ss, &sign, &tz);
    if (matched < 7) return 0;

    struct tm t{};
    t.tm_year = yy + 100;  // 2-digit year, offset from 1900
    t.tm_mon  = mo - 1;
    t.tm_mday = dd;
    t.tm_hour = hh;
    t.tm_min  = mm;
    t.tm_sec  = ss;

    time_t epoch = mktime(&t);  // NB: mktime uses local time; ensure TZ=UTC

    // Adjust for reported UTC offset (quarters of an hour)
    int offsetSec = (tz * 15 * 60);
    if (sign == '-') epoch += offsetSec;
    else             epoch -= offsetSec;

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
