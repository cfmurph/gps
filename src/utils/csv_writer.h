#pragma once

#include <stddef.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// CsvWriter
//
// Zero-allocation CSV row builder — writes directly into a caller-supplied
// buffer so no dynamic memory is needed on the hot logging path.
// ---------------------------------------------------------------------------
namespace CsvWriter {

    /**
     * Serialise a GPSRecord as a single CSV row (including trailing CRLF).
     *
     * Column order matches the header written by SdLogger:
     *   timestamp, latitude, longitude, altitude_m,
     *   speed_kmh, heading_deg, hdop, satellites, battery_pct
     *
     * @param rec    Source record (must be valid)
     * @param buf    Output buffer
     * @param bufLen Size of buf in bytes
     * @returns      Number of bytes written (excluding NUL terminator),
     *               or 0 if buf is too small.
     */
    size_t buildRow(const GPSRecord& rec, char* buf, size_t bufLen);

} // namespace CsvWriter
