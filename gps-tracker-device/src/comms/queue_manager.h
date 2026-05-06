#pragma once

#include <Arduino.h>
#include "../../include/types.h"
#include "../../include/config.h"

// ---------------------------------------------------------------------------
// QueueManager
//
// A fixed-capacity ring buffer that holds GPSRecords pending transmission.
// Designed to reside in PSRAM (allocated in begin()) so the ring can hold
// QUEUE_MAX_RECORDS entries without competing with heap-allocated objects.
//
// Thread safety: not re-entrant — call exclusively from the main loop task.
// ---------------------------------------------------------------------------
class QueueManager {
public:
    QueueManager() = default;

    /**
     * Allocate the ring buffer.
     * Attempts PSRAM first; falls back to internal heap.
     */
    void begin();

    /** Add a record to the tail of the queue. Drops the oldest if full. */
    void push(const GPSRecord& rec);

    /**
     * Peek at the record at the head of the queue without removing it.
     * Returns false if the queue is empty.
     */
    bool peek(GPSRecord& out) const;

    /** Remove the record at the head of the queue (call after successful send). */
    void pop();

    /** Number of records currently in the queue. */
    uint8_t size() const { return _count; }

    /** True when size() == 0. */
    bool empty() const { return _count == 0; }

    /** True when size() == QUEUE_MAX_RECORDS. */
    bool full() const { return _count >= QUEUE_MAX_RECORDS; }

    /** Discard all enqueued records. */
    void clear();

private:
    GPSRecord* _buf   = nullptr;
    uint8_t    _head  = 0;
    uint8_t    _tail  = 0;
    uint8_t    _count = 0;
};
