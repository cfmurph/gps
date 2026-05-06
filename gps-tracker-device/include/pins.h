#pragma once

// ---------------------------------------------------------------------------
// ESP32-WROOM-32 GPIO assignments
// Adjust to match your PCB/breadboard layout before flashing.
// ---------------------------------------------------------------------------

// --- GPS (u-blox NEO-M8N) --- UART2 ----------------------------------------
constexpr int PIN_GPS_RX   = 16;  ///< ESP32 RX2 ← GPS TX
constexpr int PIN_GPS_TX   = 17;  ///< ESP32 TX2 → GPS RX
constexpr int PIN_GPS_PPS  = 34;  ///< Pulse-per-second (input only, no pull)
constexpr int PIN_GPS_EN   = 32;  ///< Active-high module power enable

// --- LTE modem (SIMCom SIM7600) --- UART1 -----------------------------------
constexpr int PIN_LTE_RX   = 26;  ///< ESP32 RX1 ← SIM7600 TXD
constexpr int PIN_LTE_TX   = 27;  ///< ESP32 TX1 → SIM7600 RXD
constexpr int PIN_LTE_PWR  = 4;   ///< Active-high PWRKEY pulse (≥500 ms)
constexpr int PIN_LTE_RST  = 5;   ///< Active-low hardware reset
constexpr int PIN_LTE_RI   = 35;  ///< Ring indicator (input only)

// --- OLED display (SSD1306 128×64) --- I²C ----------------------------------
constexpr int PIN_OLED_SDA = 21;  ///< I²C data
constexpr int PIN_OLED_SCL = 22;  ///< I²C clock
// I²C address configured in config.h (OLED_I2C_ADDR)

// --- SD card --- SPI --------------------------------------------------------
constexpr int PIN_SD_MOSI  = 23;
constexpr int PIN_SD_MISO  = 19;
constexpr int PIN_SD_SCK   = 18;
constexpr int PIN_SD_CS    = 5;   ///< Chip-select (active low)
// Note: PIN_SD_CS and PIN_LTE_RST share GPIO5 on the default pinout.
//       Route them to different pins on custom hardware.

// --- Battery / power --------------------------------------------------------
constexpr int PIN_BATT_ADC  = 36; ///< VP (ADC1_CH0) — voltage divider mid-point
constexpr int PIN_CHRG_STAT = 39; ///< TP4056 CHRG pin — low = charging (input only)
constexpr int PIN_VBUS_DET  = 33; ///< High when USB VBUS present
