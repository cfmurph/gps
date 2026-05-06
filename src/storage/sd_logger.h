#pragma once

#include <Arduino.h>
#include <SD.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// SdLogger
//
// Appends GPS records to date-named CSV files on a microSD card.
// Handles:
//   - Lazy directory creation
//   - File rotation when SD_ROTATE_BYTES is exceeded
//   - Periodic fsync to limit data loss on power failure
//   - Graceful degradation when the card is absent or write fails
// ---------------------------------------------------------------------------
class SdLogger {
public:
    SdLogger() = default;

    /**
     * Initialise the SPI bus and mount the FAT filesystem.
     * Returns false if no card is detected — caller should disable SD logging.
     */
    bool begin();

    /** True if the card was mounted successfully and is still usable. */
    bool isAvailable() const { return _available; }

    /**
     * Write one GPS record as a CSV row.
     * Opens/rotates the file as needed.
     * Returns false on write error.
     */
    bool log(const GPSRecord& rec);

    /**
     * Force a flush of the write buffer to the card.
     * Called on a timer and on clean shutdown.
     */
    void flush();

    /** Close the current log file cleanly. */
    void close();

    /** Bytes used on the card (approximate, from FAT cluster count). */
    uint64_t usedBytes() const;

    /** Total card capacity in bytes. */
    uint64_t totalBytes() const;

private:
    File     _file;
    bool     _available    = false;
    uint32_t _lastFlushMs  = 0;
    uint32_t _currentBytes = 0;
    char     _currentPath[32]{};

    /** Build a path like /logs/2026-05-06.csv from epoch seconds. */
    void _buildPath(uint32_t epoch, char* out, size_t len);

    /** Open (or reopen after rotation) the correct log file. */
    bool _openFile(uint32_t epoch);

    /** Write the CSV header row. */
    void _writeHeader();
};
