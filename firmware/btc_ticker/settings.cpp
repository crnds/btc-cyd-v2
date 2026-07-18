#include "settings.h"
#include "candles.h"

Settings gSettings = {4, 0, 0, 0, 1, 0, 1, 0};

const uint8_t BRI_VAL[BRI_COUNT] = {13, 64, 128, 191, 255};
const char* const BRI_LABEL[BRI_COUNT] = {"5%", "25%", "50%", "75%", "100%"};

const uint32_t PRICE_IV_MS[PRICE_IV_COUNT] = {1000, 5000, 10000, 60000, 300000};
const char* const PRICE_IV_LABEL[PRICE_IV_COUNT] = {"1s", "5s", "10s", "1m", "5m"};

const uint32_t CANDLE_IV_SEC[CANDLE_IV_COUNT] = {300, 900, 3600, 14400};
const char* const CANDLE_IV_LABEL[CANDLE_IV_COUNT] = {"5m", "15m", "1h", "4h"};

const uint32_t RANGE_SEC[RANGE_COUNT] = {12UL * 3600, 24UL * 3600, 7UL * 24 * 3600};
const char* const RANGE_LABEL[RANGE_COUNT] = {"12h", "24h", "7D"};

const char* const STYLE_LABEL[STYLE_COUNT] = {"Red/Grn", "Blk/Wht", "Line"};

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
    default:             return "";
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

  if (gSettings.briIdx >= BRI_COUNT) gSettings.briIdx = 4;
  if (gSettings.flip > 1) gSettings.flip = 0;
  if (gSettings.priceIvIdx >= PRICE_IV_COUNT) gSettings.priceIvIdx = 0;
  if (gSettings.candleIvIdx >= CANDLE_IV_COUNT) gSettings.candleIvIdx = 0;
  if (gSettings.rangeIdx >= RANGE_COUNT) gSettings.rangeIdx = 1;
  if (gSettings.styleIdx >= STYLE_COUNT) gSettings.styleIdx = 0;
  if (gSettings.nightEn > 1) gSettings.nightEn = 1;
  if (gSettings.nightForce > 1) gSettings.nightForce = 0;

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
  }
}

uint8_t settingsCycle(int row) {
  uint8_t mask = 1u << row;
  switch (row) {
    case ROW_BRIGHTNESS: gSettings.briIdx = (gSettings.briIdx + 1) % BRI_COUNT; break;
    case ROW_FLIP:       gSettings.flip = gSettings.flip ? 0 : 1; break;
    case ROW_PRICE_IV:   gSettings.priceIvIdx = (gSettings.priceIvIdx + 1) % PRICE_IV_COUNT; break;
    case ROW_CANDLE_IV:  gSettings.candleIvIdx = (gSettings.candleIvIdx + 1) % CANDLE_IV_COUNT; break;
    case ROW_RANGE:      gSettings.rangeIdx = (gSettings.rangeIdx + 1) % RANGE_COUNT; break;
    case ROW_STYLE:      gSettings.styleIdx = (gSettings.styleIdx + 1) % STYLE_COUNT; break;
    case ROW_NIGHT:      gSettings.nightEn = gSettings.nightEn ? 0 : 1; break;
    case ROW_NIGHT_FORCE: gSettings.nightForce = gSettings.nightForce ? 0 : 1; break;
    default: return 0;
  }

  uint32_t count = RANGE_SEC[gSettings.rangeIdx] / CANDLE_IV_SEC[gSettings.candleIvIdx];
  if (count > (uint32_t)MAX_CANDLES) {
    if (row == ROW_CANDLE_IV) {
      gSettings.rangeIdx = 1;  // 7D -> 24h
      mask |= 1u << ROW_RANGE;
    } else {
      gSettings.candleIvIdx = 2;  // 5m/15m -> 1h
      mask |= 1u << ROW_CANDLE_IV;
    }
  }
  return mask;
}
