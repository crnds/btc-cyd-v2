#pragma once

// Pin mapping for the ESP32-2432S028R "Cheap Yellow Display" (2.8" ILI9341,
// resistive XPT2046 touch, shared VSPI bus). Capacitive (GT911) variants use
// different pins and a different LovyanGFX panel/touch config.

#define CYD_TFT_MOSI 13
#define CYD_TFT_MISO 12
#define CYD_TFT_SCLK 14
#define CYD_TFT_CS   15
#define CYD_TFT_DC   2
#define CYD_TFT_RST  -1
#define CYD_TFT_BL   21

// On-board LDR (photoresistor divider) for auto-brightness. GPIO34 is ADC1
// (safe while WiFi is up — ADC2 shares the radio). Input-only pin.
#define CYD_LDR        34

// The XPT2046 touch controller is NOT on the display's SPI bus — it has its
// own dedicated pins on this board (classic CYD gotcha).
#define CYD_TOUCH_SCLK 25
#define CYD_TOUCH_MOSI 32
#define CYD_TOUCH_MISO 39
#define CYD_TOUCH_CS   33
#define CYD_TOUCH_IRQ  36
