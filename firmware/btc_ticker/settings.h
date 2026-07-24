#pragma once
#include <stdint.h>
#include <Preferences.h>

enum SettingRow : uint8_t {
  ROW_BRIGHTNESS = 0,
  ROW_FLIP,
  ROW_PRICE_IV,
  ROW_CANDLE_IV,
  ROW_RANGE,
  ROW_STYLE,
  ROW_NIGHT,
  ROW_NIGHT_FORCE,
  ROW_RANGEBAR,
  ROW_SHOW_PRICE,
  ROW_SHOW_DATE,
  ROW_SHOW_CLOCK,
  ROW_FORGET_AP,   // action row (no value/options) — see settings.cpp switches
  ROW_COUNT
};

enum ChartStyle : uint8_t { STYLE_RG = 0, STYLE_BW = 1, STYLE_LINE = 2 };

struct Settings {
  uint8_t briIdx;       // index into BRI_VAL/BRI_LABEL, default 4 (100%)
  uint8_t flip;         // 0 = normal, 1 = rotated 180
  uint8_t priceIvIdx;   // index into PRICE_IV_MS/PRICE_IV_LABEL, default 0 (1s)
  uint8_t candleIvIdx;  // index into CANDLE_IV_SEC/CANDLE_IV_LABEL, default 0 (5m)
  uint8_t rangeIdx;     // index into RANGE_SEC/RANGE_LABEL, default 1 (24h)
  uint8_t styleIdx;     // index into STYLE_LABEL (ChartStyle), default 0 (Red/Green)
  uint8_t nightEn;      // 0 = off, 1 = red-only UI 23:00-08:00, default 1 (on)
  uint8_t nightForce;   // 0 = schedule only, 1 = force night mode on, default 0
  uint8_t rangeBar;     // 0 = hide the 24h range position bar, 1 = show, default 1
  uint8_t showPrice;    // 0 = hide the big price + 24h change, 1 = show, default 1
  uint8_t showDate;     // 0 = hide the status-bar date, 1 = show, default 1
  uint8_t showClock;    // 0 = hide the status-bar clock, 1 = show, default 1
};
extern Settings gSettings;

static const int BRI_COUNT = 7;
extern const uint8_t BRI_VAL[BRI_COUNT];
extern const char* const BRI_LABEL[BRI_COUNT];

static const int PRICE_IV_COUNT = 5;
extern const uint32_t PRICE_IV_MS[PRICE_IV_COUNT];
extern const char* const PRICE_IV_LABEL[PRICE_IV_COUNT];

static const int CANDLE_IV_COUNT = 4;
extern const uint32_t CANDLE_IV_SEC[CANDLE_IV_COUNT];
extern const char* const CANDLE_IV_LABEL[CANDLE_IV_COUNT];   // also the Binance interval strings

static const int RANGE_COUNT = 3;
extern const uint32_t RANGE_SEC[RANGE_COUNT];
extern const char* const RANGE_LABEL[RANGE_COUNT];

static const int STYLE_COUNT = 3;
extern const char* const STYLE_LABEL[STYLE_COUNT];

inline uint32_t settingsCandleSeconds() { return CANDLE_IV_SEC[gSettings.candleIvIdx]; }
inline int settingsVisibleCount() {
  return (int)(RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx]);
}
inline const char* settingsBinanceInterval() { return CANDLE_IV_LABEL[gSettings.candleIvIdx]; }
inline uint32_t settingsPriceIntervalMs() { return PRICE_IV_MS[gSettings.priceIvIdx]; }

// Label for the current value of `row`, for display in the settings list.
const char* settingsValueLabel(int row);

// Option-picker accessors: number of options for `row`, the label of option
// `idx`, and the currently selected index.
int settingsOptionCount(int row);
const char* settingsOptionLabel(int row, int idx);
uint8_t settingsOptionIndex(int row);

// Loads all settings from NVS (namespace already open on `p`), clamping any
// out-of-range stored index back to its field's default.
void settingsLoad(Preferences& p);

// Persists just the one field named by `row`.
void settingsSaveRow(Preferences& p, int row);

// Sets `row` to option `idx` (picked from the option-picker page). If the
// resulting candle-size x range combination would need more than MAX_CANDLES
// visible candles, auto-adjusts the *other* of that pair back into range
// (setting candle size snaps range down to 24h; setting range snaps candle
// size up to 1h). Returns a bitmask (1u<<row) of every row that changed, so
// the caller knows which ones to persist and re-apply; 0 if nothing changed.
// uint16_t because ROW_COUNT can exceed 8 (uint8_t would drop bits 8+).
uint16_t settingsSet(int row, uint8_t idx);
