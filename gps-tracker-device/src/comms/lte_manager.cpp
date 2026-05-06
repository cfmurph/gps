#include "lte_manager.h"
#include "../../include/config.h"
#include "../../include/pins.h"
#include <ArduinoJson.h>

// UART1 — wired to SIM7600
static HardwareSerial LteSerial(1);

// ---------------------------------------------------------------------------

LteManager::LteManager()
    : _modem(LteSerial),
      _gsmClient(_modem)
{}

// ---------------------------------------------------------------------------

bool LteManager::begin() {
    LteSerial.begin(LTE_BAUD, SERIAL_8N1, PIN_LTE_RX, PIN_LTE_TX);

    pinMode(PIN_LTE_PWR, OUTPUT);
    pinMode(PIN_LTE_RST, OUTPUT);
    digitalWrite(PIN_LTE_RST, HIGH);  // Deassert reset

    if (!_powerOn()) {
        Serial.println(F("[LTE] Modem power-on failed"));
        return false;
    }

    // Give the modem time to register
    Serial.println(F("[LTE] Waiting for network registration..."));
    if (!_modem.waitForNetwork(LTE_CONNECT_TIMEOUT_MS)) {
        Serial.println(F("[LTE] Network registration timeout"));
        return false;
    }

    return _connectBearer();
}

// ---------------------------------------------------------------------------

bool LteManager::isConnected() {
    return _modem.isNetworkConnected() && _modem.isGprsConnected();
}

// ---------------------------------------------------------------------------

bool LteManager::reconnect() {
    if (isConnected()) return true;

    Serial.println(F("[LTE] Attempting bearer reconnect..."));

    if (!_modem.isNetworkConnected()) {
        if (!_modem.waitForNetwork(LTE_CONNECT_TIMEOUT_MS)) return false;
    }

    return _connectBearer();
}

// ---------------------------------------------------------------------------

RetryResult LteManager::postRecord(const GPSRecord& rec) {
    if (!isConnected()) {
        if (!reconnect()) return RetryResult::TRANSIENT_FAIL;
    }

    // Serialise to JSON
    JsonDocument doc;
    doc["device_id"] = DEVICE_ID;
    doc["ts"]        = rec.timestamp;
    doc["lat"]       = serialized(String(rec.latitude,  6));
    doc["lon"]       = serialized(String(rec.longitude, 6));
    doc["alt_m"]     = serialized(String(rec.altitude_m,  1));
    doc["speed_kmh"] = serialized(String(rec.speed_kmh,   1));
    doc["heading"]   = serialized(String(rec.heading_deg, 1));
    doc["hdop"]      = serialized(String(rec.hdop,        2));
    doc["sats"]      = rec.satellites;
    doc["batt_pct"]  = rec.battery_pct;

    char body[256];
    serializeJson(doc, body, sizeof(body));

    HttpClient http(_gsmClient, SERVER_URL, SERVER_PORT);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.connectionKeepAlive();

    int err = http.post("/api/v1/track",
                        "application/json",
                        body);

    if (err != 0) {
        Serial.printf("[LTE] HTTP POST error: %d\n", err);
        http.stop();
        return RetryResult::TRANSIENT_FAIL;
    }

    int status = http.responseStatusCode();
    http.skipResponseHeaders();
    http.stop();

    Serial.printf("[LTE] HTTP status: %d\n", status);

    if (status >= 200 && status < 300) return RetryResult::SUCCESS;
    if (status >= 400 && status < 500) return RetryResult::PERMANENT_FAIL;
    return RetryResult::TRANSIENT_FAIL;
}

// ---------------------------------------------------------------------------

void LteManager::fillStatus(ModemStatus& out, uint8_t pendingRecords) {
    out.registered      = isConnected();
    out.rssi_dbm        = static_cast<int8_t>(_modem.getSignalQuality());
    out.pending_records = pendingRecords;
}

// ---------------------------------------------------------------------------

void LteManager::shutdown() {
    Serial.println(F("[LTE] Shutting down modem"));
    _modem.gprsDisconnect();
    _modem.radioOff();
    _powerOff();
}

// ---------------------------------------------------------------------------

bool LteManager::_powerOn() {
    // SIM7600 requires a PWRKEY pulse of ≥500 ms to power on
    Serial.println(F("[LTE] Asserting PWRKEY"));
    digitalWrite(PIN_LTE_PWR, HIGH);
    delay(600);
    digitalWrite(PIN_LTE_PWR, LOW);
    delay(3000);  // Boot time

    return _modem.restart();  // Sends AT, checks echo
}

// ---------------------------------------------------------------------------

bool LteManager::_connectBearer() {
    if (!_modem.gprsConnect(LTE_APN, LTE_USER, LTE_PASS)) {
        Serial.println(F("[LTE] GPRS bearer connect failed"));
        return false;
    }
    Serial.printf("[LTE] Bearer connected. IP: %s\n",
                  _modem.localIP().toString().c_str());
    return true;
}

// ---------------------------------------------------------------------------

void LteManager::_powerOff() {
    digitalWrite(PIN_LTE_PWR, HIGH);
    delay(600);
    digitalWrite(PIN_LTE_PWR, LOW);
}
