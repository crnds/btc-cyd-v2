#include "settings.h"
#include "candles.h"

Settings gSettings = {5, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1};

// 1% is first in order (1%, 5%, 25%, 50%, 75%, 100%, Auto).
// getAutoBrightnessVal() maps to indices 1-5 as its 5 LDR levels,
// and index 6 is hardcoded elsewhere as the "Auto" special case.
const uint8_t BRI_VAL[BRI_COUNT] = {3, 13, 64, 128, 191, 255, 0};
const char* const BRI_LABEL[BRI_COUNT] = {"1%", "5%", "25%", "50%", "75%", "100%", "Auto"};

const uint32_t PRICE_IV_MS[PRICE_IV_COUNT] = {1000, 5000, 10000, 60000, 300000};
const char* const PRICE_IV_LABEL[PRICE_IV_COUNT] = {"1s", "5s", "10s", "1m", "5m"};

const uint32_t CANDLE_IV_SEC[CANDLE_IV_COUNT] = {300, 900, 1800, 3600, 14400};
const char* const CANDLE_IV_LABEL[CANDLE_IV_COUNT] = {"5m", "15m", "30m", "1h", "4h"};

const uint32_t RANGE_SEC[RANGE_COUNT] = {12UL * 3600, 24UL * 3600, 7UL * 24 * 3600};
const char* const RANGE_LABEL[RANGE_COUNT] = {"12h", "24h", "7D"};

const char* const STYLE_LABEL[STYLE_COUNT] = {"Red/Green", "Black/White", "Line"};

static const char* const ONOFF_LABEL[2] = {"Off", "On"};

// Per-row metadata: which Settings field a row reads/writes, its option
// labels/count, and its NVS key/default. Collapses what used to be 4
// parallel switch(row) statements (one per accessor) plus a 5th in
// settingsSet and one hand-written line per field in settingsLoad/
// settingsSaveRow — adding a row now only means one new entry here, not a
// case in five different places (see AGENTS.md's Settings section).
// ROW_FORGET_AP is an action row, not a value: field/nvsKey are null and
// count is 0, so every accessor below falls through to its empty/no-op case.
struct RowMeta {
  uint8_t Settings::*field;
  const char* const* labels;
  uint8_t count;
  const char* nvsKey;
  uint8_t defaultVal;
};

static const RowMeta ROW_META[ROW_COUNT] = {
  {&Settings::briIdx,      BRI_LABEL,       BRI_COUNT,       "s.bri",  5},
  {&Settings::flip,        ONOFF_LABEL,     2,               "s.flip", 0},
  {&Settings::priceIvIdx,  PRICE_IV_LABEL,  PRICE_IV_COUNT,  "s.pIv",  0},
  {&Settings::candleIvIdx, CANDLE_IV_LABEL, CANDLE_IV_COUNT, "s.cIv",  0},
  {&Settings::rangeIdx,    RANGE_LABEL,     RANGE_COUNT,     "s.rng",  1},
  {&Settings::styleIdx,    STYLE_LABEL,     STYLE_COUNT,     "s.sty",  0},
  {&Settings::nightEn,     ONOFF_LABEL,     2,               "s.nit",  1},
  {&Settings::nightForce,  ONOFF_LABEL,     2,               "s.nf",   0},
  {&Settings::rangeBar,    ONOFF_LABEL,     2,               "s.rbar", 1},
  {&Settings::showPrice,   ONOFF_LABEL,     2,               "s.spri", 1},
  {&Settings::showDate,    ONOFF_LABEL,     2,               "s.sdat", 1},
  {&Settings::showClock,   ONOFF_LABEL,     2,               "s.sclk", 1},
  {nullptr,                nullptr,         0,               nullptr,  0},
};

const char* settingsValueLabel(int row) {
  const RowMeta& m = ROW_META[row];
  return m.field ? m.labels[gSettings.*m.field] : "";
}

int settingsOptionCount(int row) {
  return ROW_META[row].count;
}

const char* settingsOptionLabel(int row, int idx) {
  const RowMeta& m = ROW_META[row];
  return m.labels ? m.labels[idx] : "";
}

uint8_t settingsOptionIndex(int row) {
  const RowMeta& m = ROW_META[row];
  return m.field ? gSettings.*m.field : 0;
}

void settingsLoad(Preferences& p) {
  // One-time migration: "30m" was inserted at index 2 of CANDLE_IV_SEC/
  // CANDLE_IV_LABEL (old layout was {5m,15m,1h,4h}; new is {5m,15m,30m,1h,
  // 4h}), which silently reinterprets any already-persisted s.cIv >= 2 as
  // the wrong interval. Bump such a value up by one so it still names the
  // same time interval, then mark migrated so this only ever runs once. A
  // fresh device (no "s.cIv" key yet) has nothing to migrate.
  if (!p.isKey("s.civM")) {
    if (p.isKey("s.cIv")) {
      uint8_t raw = p.getUChar("s.cIv", 0);
      if (raw >= 2) p.putUChar("s.cIv", (uint8_t)(raw + 1));
    }
    p.putUChar("s.civM", 1);
  }

  for (int r = 0; r < ROW_COUNT; r++) {
    const RowMeta& m = ROW_META[r];
    if (!m.field) continue;  // action row (ROW_FORGET_AP): nothing to load
    uint8_t v = p.getUChar(m.nvsKey, m.defaultVal);
    if (v >= m.count) v = m.defaultVal;
    gSettings.*m.field = v;
  }

  // A combo persisted before this auto-adjust logic existed (or corrupted
  // NVS) could be invalid on load; fix it up the same way settingsSet does.
  if ((uint32_t)RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx] > (uint32_t)MAX_CANDLES) {
    gSettings.rangeIdx = 1;
  }
}

void settingsSaveRow(Preferences& p, int row) {
  const RowMeta& m = ROW_META[row];
  if (!m.field) return;  // action row: nothing to persist
  p.putUChar(m.nvsKey, gSettings.*m.field);
}

uint16_t settingsSet(int row, uint8_t idx) {
  if (row < 0 || row >= ROW_COUNT) return 0;
  if (idx >= (uint8_t)settingsOptionCount(row)) return 0;
  if (idx == settingsOptionIndex(row)) return 0;

  uint16_t mask = (uint16_t)(1u << row);
  gSettings.*ROW_META[row].field = idx;

  uint32_t count = RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx];
  if (count > (uint32_t)MAX_CANDLES) {
    if (row == ROW_CANDLE_IV) {
      gSettings.rangeIdx = 1;  // 7D -> 24h
      mask |= (uint16_t)(1u << ROW_RANGE);
    } else {
      gSettings.candleIvIdx = 3;  // 5m/15m/30m -> 1h
      mask |= (uint16_t)(1u << ROW_CANDLE_IV);
    }
  }
  return mask;
}
