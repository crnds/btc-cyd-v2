#include "settings.h"
#include "candles.h"

Settings gSettings = {4, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1};

const uint8_t BRI_VAL[BRI_COUNT] = {13, 64, 128, 191, 255, 0};
const char* const BRI_LABEL[BRI_COUNT] = {"5%", "25%", "50%", "75%", "100%", "Auto"};

const uint32_t PRICE_IV_MS[PRICE_IV_COUNT] = {1000, 5000, 10000, 60000, 300000};
const char* const PRICE_IV_LABEL[PRICE_IV_COUNT] = {"1s", "5s", "10s", "1m", "5m"};

const uint32_t CANDLE_IV_SEC[CANDLE_IV_COUNT] = {300, 900, 3600, 14400};
const char* const CANDLE_IV_LABEL[CANDLE_IV_COUNT] = {"5m", "15m", "1h", "4h"};

const uint32_t RANGE_SEC[RANGE_COUNT] = {12UL * 3600, 24UL * 3600, 7UL * 24 * 3600};
const char* const RANGE_LABEL[RANGE_COUNT] = {"12h", "24h", "7D"};

const char* const STYLE_LABEL[STYLE_COUNT] = {"Red/Green", "Black/White", "Line"};

static const char* ONOFF_LABEL[2] = {"Off", "On"};

const char* settingsValueLabel(int row) {
  switch (row) {
    case ROW_BRIGHTNESS: return BRI_LABEL[gSettings.briIdx];
    case ROW_FLIP:       return ONOFF_LABEL[gSettings.flip];
    case ROW_PRICE_IV:   return PRICE_IV_LABEL[gSettings.priceIvIdx];
    case ROW_CANDLE_IV:  return CANDLE_IV_LABEL[gSettings.candleIvIdx];
    case ROW_RANGE:      return RANGE_LABEL[gSettings.rangeIdx];
    case ROW_STYLE:      return STYLE_LABEL[gSettings.styleIdx];
    case ROW_NIGHT:      return ONOFF_LABEL[gSettings.nightEn];
    case ROW_NIGHT_FORCE: return ONOFF_LABEL[gSettings.nightForce];
    case ROW_RANGEBAR:   return ONOFF_LABEL[gSettings.rangeBar];
    case ROW_SHOW_PRICE: return ONOFF_LABEL[gSettings.showPrice];
    case ROW_SHOW_DATE:  return ONOFF_LABEL[gSettings.showDate];
    case ROW_SHOW_CLOCK: return ONOFF_LABEL[gSettings.showClock];
    default:             return "";
  }
}

int settingsOptionCount(int row) {
  switch (row) {
    case ROW_BRIGHTNESS: return BRI_COUNT;
    case ROW_FLIP:       return 2;
    case ROW_PRICE_IV:   return PRICE_IV_COUNT;
    case ROW_CANDLE_IV:  return CANDLE_IV_COUNT;
    case ROW_RANGE:      return RANGE_COUNT;
    case ROW_STYLE:      return STYLE_COUNT;
    case ROW_NIGHT:      return 2;
    case ROW_NIGHT_FORCE: return 2;
    case ROW_RANGEBAR:   return 2;
    case ROW_SHOW_PRICE: return 2;
    case ROW_SHOW_DATE:  return 2;
    case ROW_SHOW_CLOCK: return 2;
    default:             return 0;
  }
}

const char* settingsOptionLabel(int row, int idx) {
  switch (row) {
    case ROW_BRIGHTNESS: return BRI_LABEL[idx];
    case ROW_FLIP:       return ONOFF_LABEL[idx];
    case ROW_PRICE_IV:   return PRICE_IV_LABEL[idx];
    case ROW_CANDLE_IV:  return CANDLE_IV_LABEL[idx];
    case ROW_RANGE:      return RANGE_LABEL[idx];
    case ROW_STYLE:      return STYLE_LABEL[idx];
    case ROW_NIGHT:      return ONOFF_LABEL[idx];
    case ROW_NIGHT_FORCE: return ONOFF_LABEL[idx];
    case ROW_RANGEBAR:   return ONOFF_LABEL[idx];
    case ROW_SHOW_PRICE: return ONOFF_LABEL[idx];
    case ROW_SHOW_DATE:  return ONOFF_LABEL[idx];
    case ROW_SHOW_CLOCK: return ONOFF_LABEL[idx];
    default:             return "";
  }
}

uint8_t settingsOptionIndex(int row) {
  switch (row) {
    case ROW_BRIGHTNESS: return gSettings.briIdx;
    case ROW_FLIP:       return gSettings.flip;
    case ROW_PRICE_IV:   return gSettings.priceIvIdx;
    case ROW_CANDLE_IV:  return gSettings.candleIvIdx;
    case ROW_RANGE:      return gSettings.rangeIdx;
    case ROW_STYLE:      return gSettings.styleIdx;
    case ROW_NIGHT:      return gSettings.nightEn;
    case ROW_NIGHT_FORCE: return gSettings.nightForce;
    case ROW_RANGEBAR:   return gSettings.rangeBar;
    case ROW_SHOW_PRICE: return gSettings.showPrice;
    case ROW_SHOW_DATE:  return gSettings.showDate;
    case ROW_SHOW_CLOCK: return gSettings.showClock;
    default:             return 0;
  }
}

void settingsLoad(Preferences& p) {
  gSettings.briIdx = p.getUChar("s.bri", 4);
  gSettings.flip = p.getUChar("s.flip", 0);
  gSettings.priceIvIdx = p.getUChar("s.pIv", 0);
  gSettings.candleIvIdx = p.getUChar("s.cIv", 0);
  gSettings.rangeIdx = p.getUChar("s.rng", 1);
  gSettings.styleIdx = p.getUChar("s.sty", 0);
  gSettings.nightEn = p.getUChar("s.nit", 1);
  gSettings.nightForce = p.getUChar("s.nf", 0);
  gSettings.rangeBar = p.getUChar("s.rbar", 1);
  gSettings.showPrice = p.getUChar("s.spri", 1);
  gSettings.showDate = p.getUChar("s.sdat", 1);
  gSettings.showClock = p.getUChar("s.sclk", 1);

  if (gSettings.briIdx >= BRI_COUNT) gSettings.briIdx = 4;
  if (gSettings.flip > 1) gSettings.flip = 0;
  if (gSettings.priceIvIdx >= PRICE_IV_COUNT) gSettings.priceIvIdx = 0;
  if (gSettings.candleIvIdx >= CANDLE_IV_COUNT) gSettings.candleIvIdx = 0;
  if (gSettings.rangeIdx >= RANGE_COUNT) gSettings.rangeIdx = 1;
  if (gSettings.styleIdx >= STYLE_COUNT) gSettings.styleIdx = 0;
  if (gSettings.nightEn > 1) gSettings.nightEn = 1;
  if (gSettings.nightForce > 1) gSettings.nightForce = 0;
  if (gSettings.rangeBar > 1) gSettings.rangeBar = 1;
  if (gSettings.showPrice > 1) gSettings.showPrice = 1;
  if (gSettings.showDate > 1) gSettings.showDate = 1;
  if (gSettings.showClock > 1) gSettings.showClock = 1;

  // A combo persisted before this auto-adjust logic existed (or corrupted
  // NVS) could be invalid on load; fix it up the same way settingsCycle does.
  if ((uint32_t)RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx] > (uint32_t)MAX_CANDLES) {
    gSettings.rangeIdx = 1;
  }
}

void settingsSaveRow(Preferences& p, int row) {
  switch (row) {
    case ROW_BRIGHTNESS: p.putUChar("s.bri", gSettings.briIdx); break;
    case ROW_FLIP:       p.putUChar("s.flip", gSettings.flip); break;
    case ROW_PRICE_IV:   p.putUChar("s.pIv", gSettings.priceIvIdx); break;
    case ROW_CANDLE_IV:  p.putUChar("s.cIv", gSettings.candleIvIdx); break;
    case ROW_RANGE:      p.putUChar("s.rng", gSettings.rangeIdx); break;
    case ROW_STYLE:      p.putUChar("s.sty", gSettings.styleIdx); break;
    case ROW_NIGHT:      p.putUChar("s.nit", gSettings.nightEn); break;
    case ROW_NIGHT_FORCE: p.putUChar("s.nf", gSettings.nightForce); break;
    case ROW_RANGEBAR:   p.putUChar("s.rbar", gSettings.rangeBar); break;
    case ROW_SHOW_PRICE: p.putUChar("s.spri", gSettings.showPrice); break;
    case ROW_SHOW_DATE:  p.putUChar("s.sdat", gSettings.showDate); break;
    case ROW_SHOW_CLOCK: p.putUChar("s.sclk", gSettings.showClock); break;
  }
}

uint16_t settingsSet(int row, uint8_t idx) {
  if (row < 0 || row >= ROW_COUNT) return 0;
  if (idx >= (uint8_t)settingsOptionCount(row)) return 0;
  if (idx == settingsOptionIndex(row)) return 0;

  uint16_t mask = (uint16_t)(1u << row);
  switch (row) {
    case ROW_BRIGHTNESS: gSettings.briIdx = idx; break;
    case ROW_FLIP:       gSettings.flip = idx; break;
    case ROW_PRICE_IV:   gSettings.priceIvIdx = idx; break;
    case ROW_CANDLE_IV:  gSettings.candleIvIdx = idx; break;
    case ROW_RANGE:      gSettings.rangeIdx = idx; break;
    case ROW_STYLE:      gSettings.styleIdx = idx; break;
    case ROW_NIGHT:      gSettings.nightEn = idx; break;
    case ROW_NIGHT_FORCE: gSettings.nightForce = idx; break;
    case ROW_RANGEBAR:   gSettings.rangeBar = idx; break;
    case ROW_SHOW_PRICE: gSettings.showPrice = idx; break;
    case ROW_SHOW_DATE:  gSettings.showDate = idx; break;
    case ROW_SHOW_CLOCK: gSettings.showClock = idx; break;
  }

  uint32_t count = RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx];
  if (count > (uint32_t)MAX_CANDLES) {
    if (row == ROW_CANDLE_IV) {
      gSettings.rangeIdx = 1;  // 7D -> 24h
      mask |= (uint16_t)(1u << ROW_RANGE);
    } else {
      gSettings.candleIvIdx = 2;  // 5m/15m -> 1h
      mask |= (uint16_t)(1u << ROW_CANDLE_IV);
    }
  }
  return mask;
}
