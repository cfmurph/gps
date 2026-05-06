#include "retry_manager.h"
#include "../../include/config.h"

// ---------------------------------------------------------------------------

bool RetryManager::tick() {
    if (_queue.empty()) return false;
    // Use unsigned subtraction so the comparison is correct across the ~49.7-day
    // millis() rollover (when millis() wraps back below _nextAttemptMs).
    if (_nextAttemptMs != 0 &&
            static_cast<uint32_t>(millis() - _nextAttemptMs) > 0x7FFFFFFFUL) {
        return false;
    }

    GPSRecord rec;
    if (!_queue.peek(rec)) return false;

    RetryResult result = _lte.postRecord(rec);

    switch (result) {
        case RetryResult::SUCCESS:
            Serial.printf("[RETRY] Record ts=%lu delivered. Queue depth: %u\n",
                          static_cast<unsigned long>(rec.timestamp),
                          _queue.size() - 1);
            _queue.pop();
            _failStreak     = 0;
            _nextAttemptMs  = 0;  // Ready to send next record immediately
            return true;

        case RetryResult::TRANSIENT_FAIL:
            _failStreak = min(static_cast<int>(_failStreak) + 1,
                              static_cast<int>(RETRY_MAX_ATTEMPTS));
            _nextAttemptMs = millis() + _backoffDelay();
            Serial.printf("[RETRY] Transient failure (streak=%u). Next attempt in %lu ms\n",
                          _failStreak,
                          static_cast<unsigned long>(_backoffDelay()));
            return false;

        case RetryResult::PERMANENT_FAIL:
            Serial.printf("[RETRY] Permanent failure for ts=%lu — discarding\n",
                          static_cast<unsigned long>(rec.timestamp));
            _queue.pop();
            _failStreak = 0;
            return false;
    }

    return false;
}

// ---------------------------------------------------------------------------

bool RetryManager::isBackingOff() const {
    if (_nextAttemptMs == 0) return false;
    return static_cast<uint32_t>(millis() - _nextAttemptMs) > 0x7FFFFFFFUL;
}

// ---------------------------------------------------------------------------

uint32_t RetryManager::_backoffDelay() const {
    // delay = base * 2^failStreak, capped at max
    uint32_t delay = RETRY_BASE_DELAY_MS;
    for (uint8_t i = 0; i < _failStreak && delay < RETRY_MAX_DELAY_MS; i++) {
        delay *= 2;
        if (delay > RETRY_MAX_DELAY_MS) {
            delay = RETRY_MAX_DELAY_MS;
            break;
        }
    }
    return delay;
}
