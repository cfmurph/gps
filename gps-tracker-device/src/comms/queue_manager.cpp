#include "queue_manager.h"

// ---------------------------------------------------------------------------

void QueueManager::begin() {
    // Prefer PSRAM for the large record buffer
    size_t bytes = sizeof(GPSRecord) * QUEUE_MAX_RECORDS;

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(BOARD_HAS_PSRAM)
    _buf = static_cast<GPSRecord*>(ps_malloc(bytes));
    if (_buf) {
        Serial.printf("[QUEUE] Allocated %u bytes in PSRAM\n",
                      static_cast<unsigned>(bytes));
    }
#endif

    if (!_buf) {
        _buf = static_cast<GPSRecord*>(malloc(bytes));
        Serial.printf("[QUEUE] Allocated %u bytes in heap\n",
                      static_cast<unsigned>(bytes));
    }

    if (!_buf) {
        Serial.println(F("[QUEUE] FATAL: allocation failed"));
        // Leave _buf null — push/pop will no-op and the device can still log to SD
    }

    clear();
}

// ---------------------------------------------------------------------------

void QueueManager::push(const GPSRecord& rec) {
    if (!_buf) return;

    if (full()) {
        // Overwrite the oldest record (advance head to make room)
        Serial.println(F("[QUEUE] Overflow — dropping oldest record"));
        _head = (_head + 1) % QUEUE_MAX_RECORDS;
        _count--;
    }

    _buf[_tail] = rec;
    _tail = (_tail + 1) % QUEUE_MAX_RECORDS;
    _count++;
}

// ---------------------------------------------------------------------------

bool QueueManager::peek(GPSRecord& out) const {
    if (!_buf || empty()) return false;
    out = _buf[_head];
    return true;
}

// ---------------------------------------------------------------------------

void QueueManager::pop() {
    if (!_buf || empty()) return;
    _head = (_head + 1) % QUEUE_MAX_RECORDS;
    _count--;
}

// ---------------------------------------------------------------------------

void QueueManager::clear() {
    _head  = 0;
    _tail  = 0;
    _count = 0;
}
