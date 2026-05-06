# GPS Tracker Device

An ESP32-based GPS tracker with LTE uplink, OLED display, SD-card logging, and resilient
message delivery. Built with [PlatformIO](https://platformio.org/) and the Arduino framework.

---

## Hardware

| Component | Part | Interface |
|---|---|---|
| Microcontroller | ESP32-WROOM-32 | — |
| GNSS module | u-blox NEO-M8N | UART2 |
| LTE modem | SIMCom SIM7600E-H | UART1 |
| Display | SSD1306 128×64 OLED | I²C |
| Storage | MicroSD breakout | SPI |
| Battery | Li-Ion 3.7 V via TP4056 | ADC |

Pin assignments are defined in [`include/pins.h`](include/pins.h).

---

## Software Architecture

```
main.cpp
├── GpsManager       — NMEA parsing, fix validation, coordinate smoothing
├── DisplayManager   — U8g2 OLED pages (status, map, stats)
├── SdLogger         — CSV rotation, flush policy, file naming by date
├── BatteryManager   — ADC sampling, IIR filter, charge-state detection
├── LteManager       — SIM7600 init, bearer bring-up, HTTPS POST
├── QueueManager     — Ring-buffer of unsent GPSRecord payloads (PSRAM)
└── RetryManager     — Exponential-backoff retry loop for queued records
```

Supporting utilities live in `src/utils/`:

- **TimeUtils** — epoch ↔ human-readable conversion, NTP via modem CCLK
- **CsvWriter** — zero-allocation CSV row builder

---

## Building

```bash
# Install PlatformIO CLI if needed
pip install platformio

# Build firmware
pio run -e esp32dev

# Flash and open serial monitor
pio run -e esp32dev -t upload && pio device monitor

# Run host-side unit tests
pio test -e native
```

---

## Configuration

All tuneable parameters (server URL, transmission interval, fix timeout, …) are in
[`include/config.h`](include/config.h).  No credentials are stored in source; copy
`config.h` fields that contain secrets to a `secrets.h` that is **git-ignored**.

---

## Data Format

Each GPS record is logged to SD as a CSV row and transmitted as a JSON object:

```json
{
  "device_id": "TRACKER-001",
  "ts":        1746573600,
  "lat":       51.5074,
  "lon":       -0.1278,
  "alt_m":     32.1,
  "speed_kmh": 12.4,
  "heading":   274.0,
  "hdop":      0.9,
  "sats":      9,
  "batt_pct":  87
}
```

---

## License

MIT
