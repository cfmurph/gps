#include "sd_logger.h"
#include "../../include/config.h"
#include "../../include/pins.h"
#include "../utils/csv_writer.h"

// ---------------------------------------------------------------------------

bool SdLogger::begin() {
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println(F("[SD] Card mount failed — logging to SD disabled"));
        _available = false;
        return false;
    }

    // Ensure log directory exists
    if (!SD.exists(SD_LOG_DIR)) {
        SD.mkdir(SD_LOG_DIR);
    }

    _available = true;
    Serial.printf("[SD] Card mounted. Size: %.1f MB\n",
                  SD.totalBytes() / 1048576.0f);
    return true;
}

// ---------------------------------------------------------------------------

bool SdLogger::log(const GPSRecord& rec) {
    if (!_available) return false;
    if (!rec.valid)  return false;

    if (!_openFile(rec.timestamp)) return false;

    // Rotate if the current file exceeds the size limit
    if (_currentBytes >= SD_ROTATE_BYTES) {
        _file.close();
        // Append an index suffix to avoid clobbering today's file
        char rotated[40];
        snprintf(rotated, sizeof(rotated), "%s.%lu", _currentPath,
                 static_cast<unsigned long>(millis()));
        SD.rename(_currentPath, rotated);
        _currentBytes = 0;
        if (!_openFile(rec.timestamp)) return false;
    }

    // Build and write the CSV row
    char row[128];
    size_t len = CsvWriter::buildRow(rec, row, sizeof(row));
    if (len == 0) return false;

    size_t written = _file.print(row);
    if (written != len) {
        Serial.println(F("[SD] Write error — card full or removed?"));
        _available = false;
        return false;
    }
    _currentBytes += static_cast<uint32_t>(written);

    // Periodic fsync
    if (millis() - _lastFlushMs >= SD_FLUSH_INTERVAL_MS) {
        flush();
    }

    return true;
}

// ---------------------------------------------------------------------------

void SdLogger::flush() {
    if (_file) {
        _file.flush();
        _lastFlushMs = millis();
    }
}

// ---------------------------------------------------------------------------

void SdLogger::close() {
    if (_file) {
        _file.flush();
        _file.close();
    }
}

// ---------------------------------------------------------------------------

uint64_t SdLogger::usedBytes()  const { return _available ? SD.usedBytes()  : 0; }
uint64_t SdLogger::totalBytes() const { return _available ? SD.totalBytes() : 0; }

// ---------------------------------------------------------------------------

void SdLogger::_buildPath(uint32_t epoch, char* out, size_t len) {
    struct tm t{};
    time_t e = static_cast<time_t>(epoch);
    gmtime_r(&e, &t);
    snprintf(out, len, "%s/%04d-%02d-%02d.csv",
             SD_LOG_DIR,
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

// ---------------------------------------------------------------------------

bool SdLogger::_openFile(uint32_t epoch) {
    char path[32];
    _buildPath(epoch, path, sizeof(path));

    // Already open on the right file
    if (_file && strcmp(path, _currentPath) == 0) return true;

    // Close the previously open file before switching (day rollover)
    if (_file) _file.close();

    bool existed = SD.exists(path);
    _file = SD.open(path, FILE_APPEND);
    if (!_file) {
        Serial.printf("[SD] Cannot open %s\n", path);
        return false;
    }
    strncpy(_currentPath, path, sizeof(_currentPath) - 1);

    if (!existed) {
        _writeHeader();
        _currentBytes = 0;
    } else {
        _currentBytes = static_cast<uint32_t>(_file.size());
    }

    return true;
}

// ---------------------------------------------------------------------------

void SdLogger::_writeHeader() {
    const char* header =
        "timestamp,latitude,longitude,altitude_m,"
        "speed_kmh,heading_deg,hdop,satellites,battery_pct\r\n";
    _file.print(header);
}
