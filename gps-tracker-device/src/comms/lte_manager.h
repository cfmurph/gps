#pragma once

#include <Arduino.h>

// TinyGSM — include before the modem-specific define is evaluated
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#include "../../include/types.h"

// ---------------------------------------------------------------------------
// LteManager
//
// Owns UART1, the TinyGSM modem object, and an HTTPS client.
// Responsibilities:
//   - Power-on sequencing and AT initialisation of the SIM7600
//   - GPRS/LTE bearer management (bring-up, teardown, re-registration)
//   - HTTPS POST of a single serialised GPSRecord
//   - RSSI and registration-state reporting for display / retry decisions
// ---------------------------------------------------------------------------
class LteManager {
public:
    LteManager();

    /** Assert PWRKEY, wait for modem boot, configure APN bearer. */
    bool begin();

    /** True when the modem is registered on a network and bearer is up. */
    bool isConnected();

    /**
     * Attempt to re-establish bearer after a drop.
     * Returns true if the connection is restored within the timeout.
     */
    bool reconnect();

    /**
     * Serialise rec to JSON and POST it to SERVER_URL.
     * Returns RetryResult::SUCCESS on HTTP 200/201,
     *         RetryResult::TRANSIENT_FAIL on network/timeout errors,
     *         RetryResult::PERMANENT_FAIL on 4xx client errors.
     */
    RetryResult postRecord(const GPSRecord& rec);

    /** Fill in the caller's ModemStatus struct from current modem state. */
    void fillStatus(ModemStatus& out, uint8_t pendingRecords = 0);

    /** Gracefully detach from bearer and power down the modem. */
    void shutdown();

private:
    TinyGsm       _modem;
    TinyGsmClient _gsmClient;

    bool _powerOn();
    bool _connectBearer();
    void _powerOff();
};
