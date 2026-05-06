#include "csv_writer.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------

size_t CsvWriter::buildRow(const GPSRecord& rec, char* buf, size_t bufLen) {
    // Use snprintf to write the whole row atomically.
    // The format mirrors SdLogger::_writeHeader() exactly.
    int n = snprintf(buf, bufLen,
        "%lu,"          // timestamp
        "%.6f,"         // latitude
        "%.6f,"         // longitude
        "%.1f,"         // altitude_m
        "%.1f,"         // speed_kmh
        "%.1f,"         // heading_deg
        "%.2f,"         // hdop
        "%u,"           // satellites
        "%u"            // battery_pct
        "\r\n",
        static_cast<unsigned long>(rec.timestamp),
        rec.latitude,
        rec.longitude,
        static_cast<double>(rec.altitude_m),
        static_cast<double>(rec.speed_kmh),
        static_cast<double>(rec.heading_deg),
        static_cast<double>(rec.hdop),
        static_cast<unsigned>(rec.satellites),
        static_cast<unsigned>(rec.battery_pct));

    if (n < 0 || static_cast<size_t>(n) >= bufLen) return 0;
    return static_cast<size_t>(n);
}
