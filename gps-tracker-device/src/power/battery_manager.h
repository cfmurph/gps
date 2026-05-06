#pragma once

#include <Arduino.h>
#include "../../include/types.h"

// ---------------------------------------------------------------------------
// BatteryManager
//
// Reads cell voltage via a resistor-divider on PIN_BATT_ADC, applies an IIR
// low-pass filter to suppress ADC noise, converts to state-of-charge via a
// piecewise-linear LiPo discharge curve, and detects USB/charge-IC state.
// ---------------------------------------------------------------------------
class BatteryManager {
public:
    BatteryManager() = default;

    /** Configure ADC attenuation and seed the IIR filter with the first reading. */
    void begin();

    /**
     * Sample the ADC, update the filter, and return the latest status.
     * Inexpensive — safe to call every loop iteration.
     */
    BatteryStatus sample();

    /** Cached result of the last sample() call. */
    const BatteryStatus& status() const { return _status; }

private:
    BatteryStatus _status{};
    float         _filteredVoltage = 0.0f;
    bool          _seeded          = false;

    /** Map filtered cell voltage to an estimated SOC percentage [0, 100]. */
    static uint8_t _voltageToPct(float v);

    /** Read raw ADC, convert to volts accounting for the divider. */
    float _readVoltage() const;
};
