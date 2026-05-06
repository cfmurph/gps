#pragma once

#include <Arduino.h>
#include "../../include/types.h"
#include "lte_manager.h"
#include "queue_manager.h"

// ---------------------------------------------------------------------------
// RetryManager
//
// Drives the delivery loop: dequeues the head record, asks LteManager to POST
// it, and handles success / transient-fail / permanent-fail outcomes.
//
// Exponential backoff prevents hammering the network after repeated failures.
// The backoff state is reset on any successful delivery.
// ---------------------------------------------------------------------------
class RetryManager {
public:
    /**
     * @param lte   Reference to the initialised LteManager
     * @param queue Reference to the shared QueueManager
     */
    RetryManager(LteManager& lte, QueueManager& queue)
        : _lte(lte), _queue(queue) {}

    /**
     * Service one delivery attempt if the backoff timer has elapsed.
     * Call inside the main loop — returns immediately if not yet due.
     *
     * Returns true if a record was successfully delivered this call.
     */
    bool tick();

    /** Current number of consecutive failures (reset on success). */
    uint8_t failStreak() const { return _failStreak; }

    /** True when the manager is in backoff and not yet ready to retry. */
    bool isBackingOff() const;

private:
    LteManager&  _lte;
    QueueManager& _queue;

    uint8_t  _failStreak    = 0;
    uint32_t _nextAttemptMs = 0;

    /** Compute delay for the current fail streak (capped at RETRY_MAX_DELAY_MS). */
    uint32_t _backoffDelay() const;
};
