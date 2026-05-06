#include "battery_manager.h"
#include "../../include/config.h"
#include "../../include/pins.h"

// ---------------------------------------------------------------------------
// Piecewise-linear LiPo discharge curve.
// Pairs of {voltage_v, soc_percent} from full to empty.
// Values are empirical — refine for your specific cell chemistry.
// ---------------------------------------------------------------------------
static const float kCurveV[]  = { 4.20f, 4.10f, 4.00f, 3.90f, 3.80f,
                                   3.70f, 3.60f, 3.50f, 3.40f, 3.30f, 3.00f };
static const float kCurvePct[]= { 100.f,  95.f,  88.f,  78.f,  65.f,
                                    50.f,  35.f,  22.f,  10.f,   4.f,   0.f };
static constexpr uint8_t kCurveLen =
    static_cast<uint8_t>(sizeof(kCurveV) / sizeof(kCurveV[0]));

// ---------------------------------------------------------------------------

void BatteryManager::begin() {
    // Configure ADC1 on ESP32: 11 dB attenuation gives ~0–3.9 V full-scale
    analogSetAttenuation(ADC_11db);
    pinMode(PIN_CHRG_STAT, INPUT);
    pinMode(PIN_VBUS_DET,  INPUT);

    // Seed the filter with the actual reading to avoid a slow startup ramp
    _filteredVoltage = _readVoltage();
    _seeded = true;

    Serial.printf("[BATT] Initialised.  V=%.2f  SOC=%u%%\n",
                  _filteredVoltage, _voltageToPct(_filteredVoltage));
}

// ---------------------------------------------------------------------------

BatteryStatus BatteryManager::sample() {
    float raw = _readVoltage();

    if (!_seeded) {
        _filteredVoltage = raw;
        _seeded = true;
    } else {
        // IIR: y[n] = α·x[n] + (1-α)·y[n-1]
        _filteredVoltage = BATT_IIR_ALPHA * raw
                         + (1.0f - BATT_IIR_ALPHA) * _filteredVoltage;
    }

    _status.voltage_v = _filteredVoltage;
    _status.percent   = _voltageToPct(_filteredVoltage);
    _status.charging  = (digitalRead(PIN_CHRG_STAT) == LOW);   // TP4056 CHRG = active-low
    _status.low       = (_status.percent < LOW_BATTERY_THRESHOLD);

    return _status;
}

// ---------------------------------------------------------------------------

float BatteryManager::_readVoltage() const {
    // Average multiple samples to reduce single-shot ADC noise
    constexpr uint8_t kSamples = 8;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < kSamples; i++) {
        sum += analogRead(PIN_BATT_ADC);
    }
    float adc = static_cast<float>(sum) / kSamples;

    // ESP32 ADC: 12-bit → 4095 counts @ BATT_ADC_REF_V
    float vAdc = (adc / 4095.0f) * BATT_ADC_REF_V;

    // Undo the resistor divider to get actual cell voltage
    return vAdc * BATT_DIVIDER_RATIO;
}

// ---------------------------------------------------------------------------

uint8_t BatteryManager::_voltageToPct(float v) {
    // Clamp to the defined range
    if (v >= kCurveV[0])             return 100;
    if (v <= kCurveV[kCurveLen - 1]) return 0;

    // Linear interpolation between the nearest pair of curve points
    for (uint8_t i = 0; i < kCurveLen - 1; i++) {
        if (v <= kCurveV[i] && v >= kCurveV[i + 1]) {
            float t = (v - kCurveV[i + 1]) / (kCurveV[i] - kCurveV[i + 1]);
            float pct = kCurvePct[i + 1] + t * (kCurvePct[i] - kCurvePct[i + 1]);
            return static_cast<uint8_t>(pct + 0.5f);
        }
    }
    return 0;
}
