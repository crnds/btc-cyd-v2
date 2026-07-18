#include "ui.h"
#include "candles.h"
#include "settings.h"
#include <time.h>
#include <math.h>
#include <string.h>

// ── DESIGN SYSTEM ──────────────────────────────────────────
// 15-color palette (fits the 4-bit palette sprite's 16 entries). Color is
// spent on signal only: green/red for market direction, amber for the BTC
// brand accent and interactive affordances; everything structural stays in
// a neutral dark-gray ramp (BG < GRID < BORDER < PANEL < PANEL_HI) so data
// always outranks chrome. Contrast vs panel fills is >= 4.5:1 for all text
// colors (WCAG AA on-device).
//
// RGB565 defaults; uiUsePalette() remaps these to palette indices when the
// frame buffer is a palette sprite, so every draw call below works unchanged
// in both modes (palette index vs direct-to-panel RGB565).
// COL_BASE order doubles as the palette index order.
static const uint16_t COL_BASE[] = {
  0x0000,  // BG        #000000  screen background
  0xF7BE,  // TEXT      #F7F7F7  primary text / values
  0x9D15,  // TEXT2     #9CA2AD  secondary text / labels
  0x3ECF,  // GOOD      #3BDB7B  bull / up / healthy
  0xFA8C,  // BAD       #FF5163  bear / down / error
  0x1BC8,  // GOOD_DIM  #187B42  bull wick
  0x8967,  // BAD_DIM   #8B2D3B  bear wick
  0xFD84,  // AMBER     #FFB221  BTC accent / interactive
  0x2966,  // BORDER    #292D31  outlines, control borders
  0x7C11,  // BW_BULL_DIM #7B828C  B/W style bull body at 1-2px widths
  0x10A3,  // PANEL     #101418  card / chip fill (elevation step 1)
  0x2125,  // PANEL_HI  #212529  selected / pressed fill (elevation step 2)
  0x7411,  // TEXT3     #73828C  muted micro-labels (still >= 4.5:1 on PANEL)
  0x18E4,  // GRID      #181D21  chart gridlines / hairlines
  0x59E2,  // AMBER_DIM #5A3D10  last-price line / line-chart area fill
};
static const int COL_COUNT = sizeof(COL_BASE) / sizeof(COL_BASE[0]);

static uint16_t COL_BG       = COL_BASE[0];
static uint16_t COL_TEXT     = COL_BASE[1];
static uint16_t COL_TEXT2    = COL_BASE[2];
static uint16_t COL_GOOD     = COL_BASE[3];
static uint16_t COL_BAD      = COL_BASE[4];
static uint16_t COL_GOOD_DIM = COL_BASE[5];
static uint16_t COL_BAD_DIM  = COL_BASE[6];
static uint16_t COL_AMBER    = COL_BASE[7];
static uint16_t COL_BORDER   = COL_BASE[8];
static uint16_t COL_BW_BULL_DIM = COL_BASE[9];
static uint16_t COL_PANEL    = COL_BASE[10];
static uint16_t COL_PANEL_HI = COL_BASE[11];
static uint16_t COL_TEXT3    = COL_BASE[12];
static uint16_t COL_GRID     = COL_BASE[13];
static uint16_t COL_AMBER_DIM = COL_BASE[14];

// Non-null only in palette-sprite mode (set by uiUsePalette); null means the
// direct-to-panel fallback, where the COL_* globals hold real RGB565 values.
static lgfx::LGFX_Sprite* paletteSpr = nullptr;
static bool nightOn = false;

// Luminance in the red channel only (green/blue zeroed) — the whole UI keeps
// its contrast but renders in pure red.
static uint16_t redOnly(uint16_t c) {
  uint32_t r8 = (((c >> 11) & 0x1F) * 255 + 15) / 31;
  uint32_t g8 = (((c >> 5) & 0x3F) * 255 + 31) / 63;
  uint32_t b8 = ((c & 0x1F) * 255 + 15) / 31;
  uint32_t lum = (r8 * 299 + g8 * 587 + b8 * 114) / 1000;
  return (uint16_t)((lum & 0xF8) << 8);
}

static void applyColors(const uint16_t* rgb) {
  if (paletteSpr) {
    for (int i = 0; i < COL_COUNT; i++) paletteSpr->setPaletteColor(i, rgb[i]);
    return;
  }
  COL_BG = rgb[0];       COL_TEXT = rgb[1];     COL_TEXT2 = rgb[2];
  COL_GOOD = rgb[3];     COL_BAD = rgb[4];      COL_GOOD_DIM = rgb[5];
  COL_BAD_DIM = rgb[6];  COL_AMBER = rgb[7];    COL_BORDER = rgb[8];
  COL_BW_BULL_DIM = rgb[9];  COL_PANEL = rgb[10];  COL_PANEL_HI = rgb[11];
  COL_TEXT3 = rgb[12];   COL_GRID = rgb[13];    COL_AMBER_DIM = rgb[14];
}

// Switch the whole UI between normal and red-only colors. Cheap and
// idempotent — renderIfDue calls it every frame; only actual transitions
// touch the palette / COL_* globals. Next render picks the new colors up.
void uiSetNightMode(bool on) {
  if (on == nightOn) return;
  nightOn = on;
  static uint16_t nightCols[COL_COUNT];
  static bool nightInit = false;
  if (!nightInit) {
    for (int i = 0; i < COL_COUNT; i++) nightCols[i] = redOnly(COL_BASE[i]);
    nightInit = true;
  }
  applyColors(on ? nightCols : COL_BASE);
}

void uiUsePalette(lgfx::LGFX_Sprite& spr) {
  spr.createPalette(COL_BASE, COL_COUNT);
  paletteSpr = &spr;
  COL_BG = 0;       COL_TEXT = 1;     COL_TEXT2 = 2;   COL_GOOD = 3;
  COL_BAD = 4;      COL_GOOD_DIM = 5; COL_BAD_DIM = 6; COL_AMBER = 7;
  COL_BORDER = 8;   COL_BW_BULL_DIM = 9;  COL_PANEL = 10;  COL_PANEL_HI = 11;
  COL_TEXT3 = 12;   COL_GRID = 13;    COL_AMBER_DIM = 14;
}

// ── LAYOUT (4px grid; nothing renders past CONTENT_RIGHT) ──
static const int SCREEN_W      = 320;
static const int SCREEN_H      = 240;
static const int PAD_RIGHT     = 20;
static const int CONTENT_RIGHT = SCREEN_W - PAD_RIGHT;  // 300
static const int EDGE          = 4;  // standard left inset

// Chart footprint: fixed 288px-wide plot so the candle width is always
// >= 1px even at the maximum of MAX_CANDLES visible candles.
static const int CHART_W  = 288;
static const int CHART_Y0 = 92;   // plot top
static const int CHART_H  = 98;   // plot height, y 92..189
static const int AXIS_Y   = 190;  // x-axis hairline; labels live 196..204

// Pressed-point state for pressed-state highlights (see uiSetPressedPoint).
static int pressPtX = -1, pressPtY = -1;
static bool pressPtDown = false;

void uiSetPressedPoint(int x, int y, bool down) {
  pressPtX = x;
  pressPtY = y;
  pressPtDown = down;
}

static bool pressedIn(int x0, int y0, int x1, int y1) {
  return pressPtDown && pressPtX >= x0 && pressPtX < x1 &&
         pressPtY >= y0 && pressPtY < y1;
}

// Resolve visible candle i (0 = oldest, count-1 = newest/forming) straight
// from the ring — no per-render snapshot buffer. Everything runs on the
// single loop() task, so the ring can't mutate between drawChart's range
// pass and its draw pass.
static bool visibleCandle(int i, int count, bool haveBucket,
                          uint32_t currentBucket, CandleRec& out) {
  if (!haveBucket) {
    // pre-NTP fallback: paint whatever's on flash in raw slot order so the
    // chart isn't blank for the second or two before time syncs
    if (candleRing[i].openEpoch == 0) return false;
    out = candleRing[i];
    return true;
  }
  int bucketsAgo = (count - 1) - i;
  if (bucketsAgo == 0) {
    out = formingCandle;
    return true;
  }
  uint32_t openEpoch = (currentBucket - (uint32_t)bucketsAgo) * candleSeconds;
  const CandleRec& rec = candleRing[candleSlot(openEpoch)];
  if (rec.openEpoch != openEpoch) return false;
  out = rec;
  return true;
}

static String fmtCommas(long v) {
  bool neg = v < 0;
  if (neg) v = -v;
  String s = String(v);
  int n = s.length();
  int firstGroup = n % 3;
  if (firstGroup == 0) firstGroup = 3;
  String out = s.substring(0, firstGroup);
  for (int i = firstGroup; i < n; i += 3) {
    out += ",";
    out += s.substring(i, i + 3);
  }
  return neg ? "-" + out : out;
}

// Micro-labels in tracked capitals (+1px letter spacing) — the "section
// label" voice of the type system. Size-1 GLCD glyphs, drawn char by char.
static int capsWidth(const char* s) {
  int n = strlen(s);
  return n > 0 ? n * 7 - 1 : 0;
}

static void drawCaps(lgfx::LovyanGFX* g, int x, int y, const char* s, uint16_t color) {
  g->setTextSize(1);
  g->setTextColor(color);
  char buf[2] = {0, 0};
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c >= 'a' && c <= 'z') c -= 32;
    buf[0] = c;
    g->setCursor(x, y);
    g->print(buf);
    x += 7;
  }
}

static void drawWifi(lgfx::LovyanGFX* g, int32_t x, int32_t y, uint16_t color) {
  g->fillCircle(x, y, 1, color);
  g->drawArc(x, y, 3, 3, 225.0f, 315.0f, color);
  g->drawArc(x, y, 6, 6, 225.0f, 315.0f, color);
  g->drawArc(x, y, 9, 9, 225.0f, 315.0f, color);
}

static void drawGear(lgfx::LovyanGFX* g, int cx, int cy, uint16_t col) {
  static const int8_t tx[8] = {8, 6, 0, -6, -8, -6, 0, 6};
  static const int8_t ty[8] = {0, 6, 8, 6, 0, -6, -8, -6};
  for (int i = 0; i < 8; i++) g->fillRect(cx + tx[i] - 1, cy + ty[i] - 1, 3, 3, col);
  g->fillCircle(cx, cy, 6, col);
  g->fillCircle(cx, cy, 2, COL_BG);  // hub hole
}

// ── STATUS BAR (y 0..24) ───────────────────────────────────
// Asset tag left, clock centered, date + wifi right. Connectivity lives
// here (chrome), data freshness lives in the footer (next to the chart).
static void drawStatusBar(lgfx::LovyanGFX* g, const UiState& st) {
  drawCaps(g, EDGE, 8, "BTC/USDT", COL_TEXT3);

  struct tm t;
  bool haveTime = getLocalTime(&t, 0);
  if (haveTime) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    char sbuf[4];
    snprintf(sbuf, sizeof(sbuf), ":%02d", t.tm_sec);

    g->setTextSize(2);
    int w1 = g->textWidth(buf);
    g->setTextSize(1);
    int w2 = g->textWidth(sbuf);
    int startX = (SCREEN_W - w1 - w2) / 2;

    g->setTextSize(2);
    g->setTextColor(COL_TEXT);
    g->setCursor(startX, 4);
    g->print(buf);

    g->setTextSize(1);
    g->setTextColor(COL_TEXT3);
    g->setCursor(startX + w1, 11);
    g->print(sbuf);

    static const char* wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mons[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char dbuf[20];
    snprintf(dbuf, sizeof(dbuf), "%s %d %s", wdays[t.tm_wday], t.tm_mday, mons[t.tm_mon]);
    g->setTextSize(1);
    g->setTextColor(COL_TEXT2);
    int dateW = g->textWidth(dbuf);
    g->setCursor(CONTENT_RIGHT - 24 - dateW, 8);  // 24px reserved for the wifi glyph
    g->print(dbuf);
  } else {
    g->setTextSize(2);
    g->setTextColor(COL_TEXT3);
    int w = g->textWidth("--:--");
    g->setCursor((SCREEN_W - w) / 2, 4);
    g->print("--:--");
  }

  // WiFi glyph, right corner: radii 3/6/9 keep it inside CONTENT_RIGHT.
  drawWifi(g, CONTENT_RIGHT - 10, 14, st.wifiConnected ? COL_GOOD : COL_BAD);

  g->drawFastHLine(EDGE, 24, CONTENT_RIGHT - EDGE, COL_GRID);
}

// ── PRICE HERO (y 30..66) ──────────────────────────────────
// Left-aligned (reading gravity). The 24h change is a filled directional
// chip — the one element besides the price meant to read at arm's length.
static void drawPriceRow(lgfx::LovyanGFX* g, const UiState& st) {
  String valStr = "--";
  if (!isnan(st.price)) valStr = fmtCommas((long)(st.price + 0.5f));

  g->setTextSize(2);
  int wDollar = g->textWidth("$");
  g->setTextSize(4);
  int wVal = g->textWidth(valStr);

  // "$" baseline-aligned with the digits (both bottoms at y=66).
  g->setTextSize(2);
  g->setTextColor(isnan(st.price) ? COL_TEXT3 : COL_TEXT2);
  g->setCursor(EDGE, 50);
  g->print("$");

  g->setTextSize(4);
  g->setTextColor(isnan(st.price) ? COL_TEXT3 : COL_TEXT);
  g->setCursor(EDGE + wDollar + 2, 34);
  g->print(valStr);

  // Change chip (or a neutral state chip before the first payload).
  char chipBuf[16];
  uint16_t chipFill, chipText;
  if (!isnan(st.changePct)) {
    snprintf(chipBuf, sizeof(chipBuf), "%+.2f%%", st.changePct);
    chipFill = st.changePct >= 0 ? COL_GOOD : COL_BAD;
    chipText = COL_BG;
  } else {
    // Empty state: say what the device is doing instead of showing "--".
    snprintf(chipBuf, sizeof(chipBuf), "%s", st.wifiConnected ? "CONNECTING" : "OFFLINE");
    chipFill = COL_PANEL;
    chipText = st.wifiConnected ? COL_TEXT2 : COL_BAD;
  }
  g->setTextSize(2);
  int chipTextW = g->textWidth(chipBuf);
  int chipW = chipTextW + 16;
  int chipX = EDGE + wDollar + 2 + wVal + 10;
  if (chipX + chipW > CONTENT_RIGHT) chipX = CONTENT_RIGHT - chipW;  // extreme prices
  g->fillRoundRect(chipX, 39, chipW, 22, 6, chipFill);
  g->setTextColor(chipText);
  g->setCursor(chipX + 8, 43);
  g->print(chipBuf);
}

// ── 24H RANGE BAR (y 72..80) ───────────────────────────────
// Full-width `low ────●──── high` track with the current price as the dot.
// Skipped until the first ticker payload arrives (or on a degenerate
// high <= low), matching the chart's stay-blank-when-no-data behavior.
static void drawRangeBar(lgfx::LovyanGFX* g, const UiState& st) {
  if (!gSettings.rangeBar) return;  // hidden via the "Range bar" settings row
  if (isnan(st.price) || isnan(st.dayHigh) || isnan(st.dayLow)) return;
  float span = st.dayHigh - st.dayLow;
  if (span <= 0) return;

  g->setTextSize(1);
  String lowStr = fmtCommas((long)(st.dayLow + 0.5f));
  String highStr = fmtCommas((long)(st.dayHigh + 0.5f));
  int wLow = g->textWidth(lowStr);
  int wHigh = g->textWidth(highStr);

  const int Y_TXT = 72;
  const int Y_TRK = 76;  // vertically centered on the 8px-tall labels
  int xL = EDGE + wLow + 8;
  int xH = CONTENT_RIGHT - wHigh - 8;

  g->setTextColor(COL_TEXT2);
  g->setCursor(EDGE, Y_TXT);
  g->print(lowStr);
  g->setCursor(CONTENT_RIGHT - wHigh, Y_TXT);
  g->print(highStr);

  g->drawFastHLine(xL, Y_TRK, xH - xL, COL_BORDER);
  g->drawFastVLine(xL, Y_TRK - 3, 7, COL_TEXT2);      // low end tick
  g->drawFastVLine(xH - 1, Y_TRK - 3, 7, COL_TEXT2);  // high end tick

  float f = (st.price - st.dayLow) / span;
  if (f < 0) f = 0;  // clamp: the 24h window rolls, so a freshly printed
  if (f > 1) f = 1;  // price can briefly sit outside it — pin the dot to an end
  g->fillCircle(xL + (int)(f * (xH - xL - 1)), Y_TRK, 3, COL_AMBER);
}

// ── CHART (plot y 92..189, axis strip 190..206) ────────────
static void drawChart(lgfx::LovyanGFX* g, const UiState& st) {
  int count = settingsVisibleCount();
  if (count < 1) count = 1;
  if (count > MAX_CANDLES) count = MAX_CANDLES;
  int cw = CHART_W / count;
  if (cw < 1) cw = 1;
  int plotW = cw * count;
  int x0 = CONTENT_RIGHT - 2 - plotW;  // right edge lands just inside CONTENT_RIGHT

  bool haveBucket = formingCandle.openEpoch != 0;
  uint32_t currentBucket = haveBucket ? formingCandle.openEpoch / candleSeconds : 0;

  float rangeMin = 1e18f, rangeMax = -1e18f;
  bool haveRange = false;
  for (int i = 0; i < count; i++) {
    CandleRec r;
    if (!visibleCandle(i, count, haveBucket, currentBucket, r)) continue;
    if (r.l < rangeMin) rangeMin = r.l;
    if (r.h > rangeMax) rangeMax = r.h;
    haveRange = true;
  }

  if (!haveRange) {
    // Empty state (fresh boot with a wiped DB and no feed yet): name the
    // state instead of leaving a void between the range bar and the footer.
    const char* msg = st.wifiConnected ? "WAITING FOR MARKET FEED" : "OFFLINE — NO CACHED DATA";
    drawCaps(g, x0 + (plotW - capsWidth(msg)) / 2, CHART_Y0 + CHART_H / 2 - 4, msg, COL_TEXT3);
    return;
  }
  float span = rangeMax - rangeMin;
  if (span < 1.0f) span = 1.0f;  // avoid div-by-zero on a dead-flat market

  auto priceToY = [&](float p) -> int {
    float f = (p - rangeMin) / span;
    int y = CHART_Y0 + CHART_H - 1 - (int)(f * (CHART_H - 1));
    if (y < CHART_Y0) y = CHART_Y0;
    if (y > CHART_Y0 + CHART_H - 1) y = CHART_Y0 + CHART_H - 1;
    return y;
  };

  // Horizontal gridlines at min / mid / max, drawn first so candles,
  // labels and the last-price marker always sit on top of them.
  int yMax = priceToY(rangeMax);
  int yMin = priceToY(rangeMin);
  int yMid = priceToY((rangeMax + rangeMin) * 0.5f);
  g->drawFastHLine(x0, yMax, plotW, COL_GRID);
  g->drawFastHLine(x0, yMid, plotW, COL_GRID);
  g->drawFastHLine(x0, yMin, plotW, COL_GRID);

  ChartStyle style = (ChartStyle)gSettings.styleIdx;

  if (style == STYLE_LINE) {
    // Dim area fill first, then a 2px amber line on top.
    for (int i = 0; i < count; i++) {
      CandleRec r;
      if (!visibleCandle(i, count, haveBucket, currentBucket, r)) continue;
      int x = x0 + i * cw + cw / 2;
      int y = priceToY(r.c);
      g->drawFastVLine(x, y + 1, CHART_Y0 + CHART_H - 1 - y, COL_AMBER_DIM);
    }
    bool havePrev = false;
    int prevX = 0, prevY = 0;
    for (int i = 0; i < count; i++) {
      CandleRec r;
      if (!visibleCandle(i, count, haveBucket, currentBucket, r)) { havePrev = false; continue; }
      int x = x0 + i * cw + cw / 2;
      int y = priceToY(r.c);
      if (havePrev) {
        g->drawLine(prevX, prevY, x, y, COL_AMBER);
        g->drawLine(prevX, prevY + 1, x, y + 1, COL_AMBER);
      } else {
        g->drawPixel(x, y, COL_AMBER);
        g->drawPixel(x, y + 1, COL_AMBER);
      }
      prevX = x;
      prevY = y;
      havePrev = true;
    }
  } else {
    for (int i = 0; i < count; i++) {
      CandleRec r;
      if (!visibleCandle(i, count, haveBucket, currentBucket, r)) continue;
      int x = x0 + i * cw;
      int wickX = x + cw / 2;
      bool bull = r.c >= r.o;
      bool forming = haveBucket && (i == count - 1);

      int yH = priceToY(r.h);
      int yL = priceToY(r.l);
      if (yL < yH) { int tmp = yH; yH = yL; yL = tmp; }

      int yO = priceToY(r.o);
      int yC = priceToY(r.c);
      int yTop = min(yO, yC);
      int bodyLen = max(yO, yC) - yTop + 1;
      int bodyW = cw >= 3 ? cw - 1 : cw;

      if (style == STYLE_RG) {
        g->drawFastVLine(wickX, yH, yL - yH + 1, bull ? COL_GOOD_DIM : COL_BAD_DIM);
        if (forming && cw >= 3) {
          // The live candle is hollow — an outline says "still forming"
          // without a legend or extra color.
          g->drawRect(x, yTop, bodyW, bodyLen, bull ? COL_GOOD : COL_BAD);
        } else {
          g->fillRect(x, yTop, bodyW, bodyLen, bull ? COL_GOOD : COL_BAD);
        }
      } else {  // STYLE_BW
        g->drawFastVLine(wickX, yH, yL - yH + 1, COL_TEXT);
        if (bull) {
          if (cw >= 3) g->drawRect(x, yTop, bodyW, bodyLen, COL_TEXT);
          else g->fillRect(x, yTop, bodyW, bodyLen, COL_BW_BULL_DIM);
        } else {
          g->fillRect(x, yTop, bodyW, bodyLen, COL_TEXT);
        }
      }
    }
  }

  // Y-axis labels (min / mid / max) as BG-backed tags inside the left edge —
  // readable over any candle pattern without reserving a permanent gutter.
  g->setTextSize(1);
  g->setTextColor(COL_TEXT2);
  long mids[3] = {(long)(rangeMax + 0.5f), (long)((rangeMax + rangeMin) * 0.5f + 0.5f),
                  (long)(rangeMin + 0.5f)};
  int ys[3] = {yMax + 2, yMid - 4, yMin - 9};
  for (int k = 0; k < 3; k++) {
    String s = fmtCommas(mids[k]);
    int w = g->textWidth(s);
    g->fillRect(x0, ys[k] - 1, w + 5, 10, COL_BG);
    g->setCursor(x0 + 2, ys[k]);
    g->print(s);
  }

  // Last-price marker: dashed dim-amber line across the plot + a price tag
  // on the right edge. Anchors "where is the price now" inside the shape.
  if (!isnan(st.price)) {
    int yP = priceToY(st.price);
    if (yP < CHART_Y0 + 5) yP = CHART_Y0 + 5;
    if (yP > CHART_Y0 + CHART_H - 7) yP = CHART_Y0 + CHART_H - 7;
    String tag = fmtCommas((long)(st.price + 0.5f));
    int tagW = g->textWidth(tag);
    for (int x = x0; x < x0 + plotW - tagW - 8; x += 7) {
      g->drawFastHLine(x, yP, 4, COL_AMBER_DIM);
    }
    g->fillRect(x0 + plotW - tagW - 6, yP - 5, tagW + 6, 11, COL_BG);
    g->setTextColor(COL_AMBER);
    g->setCursor(x0 + plotW - tagW - 4, yP - 4);
    g->print(tag);
  }

  // X-axis time ticks: 12h -> every 2h, 24h -> every 4h, 7D -> daily.
  // Labels sit in the dedicated strip below the plot so they never fight
  // the candles.
  uint32_t tickSec = 7200;
  bool dayLabels = false;
  if (gSettings.rangeIdx == 1) tickSec = 14400;
  else if (gSettings.rangeIdx == 2) { tickSec = 86400; dayLabels = true; }

  g->drawFastHLine(x0, AXIS_Y, plotW, COL_GRID);
  g->setTextSize(1);
  g->setTextColor(COL_TEXT3);
  for (int i = 0; i < count; i++) {
    CandleRec r;
    if (!visibleCandle(i, count, haveBucket, currentBucket, r)) continue;
    if (r.openEpoch % tickSec != 0) continue;
    int cx = x0 + i * cw + cw / 2;
    g->drawFastVLine(cx, AXIS_Y, 4, COL_BORDER);
    time_t tt = (time_t)r.openEpoch;
    struct tm* tv = localtime(&tt);
    if (!tv) continue;
    char lb[4];
    if (dayLabels) snprintf(lb, sizeof(lb), "%d", tv->tm_mday);
    else snprintf(lb, sizeof(lb), "%02d", tv->tm_hour);
    int w = g->textWidth(lb);
    int lx = cx - w / 2;
    if (lx < x0) lx = x0;
    if (lx + w > x0 + plotW) lx = x0 + plotW - w;
    g->setCursor(lx, AXIS_Y + 6);
    g->print(lb);
  }
}

// ── FOOTER (y 212..238) ────────────────────────────────────
// Feed freshness as labeled text (not a bare colored dot — status must not
// depend on color alone), device stats demoted to a muted micro-line, and
// the settings gear as a real outlined button.
static void drawFooter(lgfx::LovyanGFX* g, const UiState& st) {
  g->drawFastHLine(EDGE, 212, CONTENT_RIGHT - EDGE, COL_GRID);

  uint32_t now = millis();
  const char* txt;
  uint16_t col;
  bool pulse = false;
  char staleBuf[12];
  if (!st.wifiConnected) {
    txt = "OFFLINE";
    col = COL_BAD;
  } else if (st.priceOkMs == 0) {
    txt = "CONNECTING";
    col = COL_TEXT2;
    pulse = true;
  } else {
    // Freshness is judged against the configured poll cadence: at a 5m
    // interval a 45s-old price is still "live", not "stale".
    uint32_t age = now - st.priceOkMs;
    uint32_t freshMs = 2 * settingsPriceIntervalMs() + 3000;
    uint32_t staleMs = 4 * settingsPriceIntervalMs() + 30000;
    if (age <= freshMs) {
      txt = "LIVE";
      col = COL_GOOD;
      pulse = true;
    } else {
      col = age <= staleMs ? COL_AMBER : COL_BAD;
      if (age < 60000) snprintf(staleBuf, sizeof(staleBuf), "STALE %us", (unsigned)(age / 1000));
      else snprintf(staleBuf, sizeof(staleBuf), "STALE %um", (unsigned)(age / 60000));
      txt = staleBuf;
    }
  }

  bool dotOn = !pulse || ((now / 1000) % 2 == 0);
  if (dotOn) g->fillCircle(7, 222, 3, col);
  drawCaps(g, 14, 219, txt, col);

  char stats[32];
  snprintf(stats, sizeof(stats), "CPU %02u%%  RAM %02u%%  ROM %02u%%",
           (unsigned)st.cpuPct, (unsigned)st.ramPct, (unsigned)st.romPct);
  g->setTextSize(1);
  g->setTextColor(COL_TEXT3);
  g->setCursor(EDGE, 230);
  g->print(stats);

  // Settings button: outlined hit-target look, flush with CONTENT_RIGHT.
  bool pressed = pressedIn(UI_GEAR_HIT_X0, UI_GEAR_HIT_Y0, SCREEN_W, SCREEN_H);
  if (pressed) g->fillRoundRect(CONTENT_RIGHT - 32, 216, 28, 20, 5, COL_PANEL_HI);
  g->drawRoundRect(CONTENT_RIGHT - 32, 216, 28, 20, 5, COL_BORDER);
  drawGear(g, CONTENT_RIGHT - 18, 226, COL_TEXT2);
}

void uiRender(lgfx::LovyanGFX* g, const UiState& st) {
  g->fillScreen(COL_BG);
  drawStatusBar(g, st);
  drawPriceRow(g, st);
  drawRangeBar(g, st);
  drawChart(g, st);
  drawFooter(g, st);
}

// ── SETTINGS: MODEL ────────────────────────────────────────
static const char* const SETTINGS_ROW_LABELS[ROW_COUNT] = {
  "Brightness", "Flip screen", "Price fetch", "Candle size", "Time range", "Chart style",
  "Night schedule", "Force night mode", "Range bar"
};

// One-line explanation shown on each option-picker page.
static const char* const SETTINGS_ROW_DESC[ROW_COUNT] = {
  "Backlight level. Auto follows ambient light.",
  "Rotate the screen 180 degrees.",
  "How often Binance is polled for the price.",
  "Minutes per candle. Changing clears history.",
  "Total time shown on the chart.",
  "Candle colors, monochrome, or a line chart.",
  "Dim red-only screen from 23:00 to 08:00.",
  "Keep night mode on, ignoring the schedule.",
  "Show today's low-to-high position bar."
};

// Visual order of the grouped list — independent of the ROW_* enum so the
// IA can evolve without touching settings logic. Binary rows (2 options)
// toggle in place; wider rows open the picker.
struct SetItem { uint8_t row; const char* header; };
static const SetItem SET_ITEMS[] = {
  {0xFF, "MARKET DATA"},
  {ROW_PRICE_IV, nullptr},
  {ROW_CANDLE_IV, nullptr},
  {ROW_RANGE, nullptr},
  {0xFF, "CHART"},
  {ROW_STYLE, nullptr},
  {ROW_RANGEBAR, nullptr},
  {0xFF, "DISPLAY"},
  {ROW_BRIGHTNESS, nullptr},
  {ROW_FLIP, nullptr},
  {ROW_NIGHT, nullptr},
  {ROW_NIGHT_FORCE, nullptr},
};
static const int SET_ITEM_COUNT = sizeof(SET_ITEMS) / sizeof(SET_ITEMS[0]);
static const int SET_HDR_H = 26;
static const int SET_ROW_H = 40;

static int setItemTop(int i) {
  int y = UI_SET_TITLE_H;
  for (int k = 0; k < i; k++) y += SET_ITEMS[k].header ? SET_HDR_H : SET_ROW_H;
  return y;
}

int uiSettingsMaxScroll() {
  int total = setItemTop(SET_ITEM_COUNT) - UI_SET_TITLE_H;
  int max = total - (SCREEN_H - UI_SET_TITLE_H);
  return max > 0 ? max : 0;
}

int uiSettingsItemAt(int scrollPx, int ty) {
  if (ty < UI_SET_TITLE_H) return -1;
  for (int i = 0; i < SET_ITEM_COUNT; i++) {
    int h = SET_ITEMS[i].header ? SET_HDR_H : SET_ROW_H;
    int y = setItemTop(i) - scrollPx;
    if (ty >= y && ty < y + h) return SET_ITEMS[i].header ? -1 : SET_ITEMS[i].row;
  }
  return -1;
}

// Picker geometry: description at the top, options below it.
static const int PICK_OPTS_Y = 52;

static int pickerRowH(int n) {
  if (n <= 4) return 40;
  return (SCREEN_H - PICK_OPTS_Y) / n;
}

int uiPickerOptionAt(int row, int ty) {
  int n = settingsOptionCount(row);
  if (n < 1 || ty < PICK_OPTS_Y) return -1;
  int idx = (ty - PICK_OPTS_Y) / pickerRowH(n);
  return idx < n ? idx : -1;
}

// ── SETTINGS: CHROME ───────────────────────────────────────
// "< " back affordance + page title, left-aligned; hairline below.
static void drawSettingsTitle(lgfx::LovyanGFX* g, const char* title) {
  g->setTextSize(2);
  g->setTextColor(COL_AMBER);
  g->setCursor(8, 7);
  g->print("<");
  g->setTextColor(COL_TEXT);
  g->setCursor(28, 7);
  g->print(title);
  g->drawFastHLine(0, UI_SET_TITLE_H - 1, CONTENT_RIGHT, COL_GRID);
}

static void drawToggle(lgfx::LovyanGFX* g, int x, int y, bool on) {
  if (on) {
    g->fillRoundRect(x, y, 30, 16, 8, COL_AMBER);
    g->fillCircle(x + 22, y + 8, 5, COL_BG);
  } else {
    g->fillRoundRect(x, y, 30, 16, 8, COL_PANEL_HI);
    g->fillCircle(x + 8, y + 8, 5, COL_TEXT2);
  }
}

void uiRenderSettings(lgfx::LovyanGFX* g, int scrollPx) {
  g->fillScreen(COL_BG);

  // Rows scroll under the fixed title bar — clip so a half-scrolled row
  // can't paint over it.
  g->setClipRect(0, UI_SET_TITLE_H, SCREEN_W, SCREEN_H - UI_SET_TITLE_H);

  for (int i = 0; i < SET_ITEM_COUNT; i++) {
    bool isHdr = SET_ITEMS[i].header != nullptr;
    int h = isHdr ? SET_HDR_H : SET_ROW_H;
    int y = setItemTop(i) - scrollPx;
    if (y + h <= UI_SET_TITLE_H || y >= SCREEN_H) continue;

    if (isHdr) {
      drawCaps(g, 12, y + 10, SET_ITEMS[i].header, COL_TEXT3);
      int lx = 12 + capsWidth(SET_ITEMS[i].header) + 8;
      g->drawFastHLine(lx, y + 13, CONTENT_RIGHT - lx, COL_GRID);
      continue;
    }

    int row = SET_ITEMS[i].row;
    if (pressedIn(0, y, CONTENT_RIGHT, y + h)) {
      g->fillRect(0, y, CONTENT_RIGHT, h, COL_PANEL_HI);
    }

    // Label in the secondary voice; the value is the information, so it
    // gets the primary color.
    g->setTextSize(1);
    g->setTextColor(COL_TEXT2);
    g->setCursor(12, y + (SET_ROW_H - 8) / 2);
    g->print(SETTINGS_ROW_LABELS[row]);

    if (settingsOptionCount(row) == 2) {
      // Binary setting: toggle switch, taps flip it in place (no picker).
      drawToggle(g, CONTENT_RIGHT - 42, y + 12, settingsOptionIndex(row) == 1);
    } else {
      const char* val = settingsValueLabel(row);
      int vw = g->textWidth(val);
      int chevX = CONTENT_RIGHT - 16;
      g->setTextColor(COL_AMBER);
      g->setCursor(chevX, y + (SET_ROW_H - 8) / 2);
      g->print(">");
      g->setTextColor(COL_TEXT);
      g->setCursor(chevX - 8 - vw, y + (SET_ROW_H - 8) / 2);
      g->print(val);
    }

    g->drawFastHLine(12, y + SET_ROW_H - 1, CONTENT_RIGHT - 12, COL_GRID);
  }

  // Scrollbar thumb, flush against CONTENT_RIGHT.
  int maxScroll = uiSettingsMaxScroll();
  if (maxScroll > 0) {
    int viewH = SCREEN_H - UI_SET_TITLE_H;
    int listH = viewH + maxScroll;
    int thumbH = viewH * viewH / listH;
    if (thumbH < 12) thumbH = 12;
    int thumbY = UI_SET_TITLE_H + (viewH - thumbH) * scrollPx / maxScroll;
    g->fillRect(CONTENT_RIGHT - 2, thumbY, 2, thumbH, COL_BORDER);
  }

  g->clearClipRect();
  drawSettingsTitle(g, "Settings");
}

void uiRenderSettingsPicker(lgfx::LovyanGFX* g, int row) {
  g->fillScreen(COL_BG);
  drawSettingsTitle(g, SETTINGS_ROW_LABELS[row]);

  g->setTextSize(1);
  g->setTextColor(COL_TEXT2);
  g->setCursor(12, 38);
  g->print(SETTINGS_ROW_DESC[row]);

  int n = settingsOptionCount(row);
  int cur = settingsOptionIndex(row);
  int rowH = pickerRowH(n);
  for (int i = 0; i < n; i++) {
    int y = PICK_OPTS_Y + i * rowH;
    int cy = y + rowH / 2;
    bool sel = i == cur;

    if (sel) g->fillRect(0, y, CONTENT_RIGHT, rowH, COL_PANEL_HI);
    else if (pressedIn(0, y, CONTENT_RIGHT, y + rowH)) {
      g->fillRect(0, y, CONTENT_RIGHT, rowH, COL_PANEL);
    }

    if (sel) {
      g->drawCircle(20, cy, 6, COL_AMBER);
      g->fillCircle(20, cy, 3, COL_AMBER);
    } else {
      g->drawCircle(20, cy, 6, COL_TEXT3);
    }

    g->setTextSize(2);
    g->setTextColor(sel ? COL_TEXT : COL_TEXT2);
    g->setCursor(40, cy - 8);
    g->print(settingsOptionLabel(row, i));

    g->drawFastHLine(12, y + rowH - 1, CONTENT_RIGHT - 12, COL_GRID);
  }
}

void uiRenderConfirm(lgfx::LovyanGFX* g, int row, int idx) {
  g->fillScreen(COL_BG);

  // Warning glyph.
  g->drawCircle(160, 56, 16, COL_AMBER);
  g->setTextSize(2);
  g->setTextColor(COL_AMBER);
  g->setCursor(154, 48);
  g->print("!");

  const char* title = "CLEAR CHART HISTORY?";
  g->setTextColor(COL_TEXT);
  g->setCursor((SCREEN_W - g->textWidth(title)) / 2, 84);
  g->print(title);

  g->setTextSize(1);
  g->setTextColor(COL_TEXT2);
  const char* l1 = "Changing the candle size erases the";
  const char* l2 = "stored chart history and re-downloads it.";
  g->setCursor((SCREEN_W - g->textWidth(l1)) / 2, 106);
  g->print(l1);
  g->setCursor((SCREEN_W - g->textWidth(l2)) / 2, 116);
  g->print(l2);

  // The pending change, e.g. "5m -> 15m".
  char preview[24];
  snprintf(preview, sizeof(preview), "%s -> %s",
           settingsOptionLabel(row, settingsOptionIndex(row)),
           settingsOptionLabel(row, idx));
  g->setTextSize(2);
  g->setTextColor(COL_AMBER);
  g->setCursor((SCREEN_W - g->textWidth(preview)) / 2, 132);
  g->print(preview);

  // Buttons. Hit zones live in ui.h (UI_CONFIRM_*).
  if (pressedIn(UI_CONFIRM_CANCEL_X0, UI_CONFIRM_Y0, UI_CONFIRM_CANCEL_X1, UI_CONFIRM_Y1)) {
    g->fillRoundRect(UI_CONFIRM_CANCEL_X0, UI_CONFIRM_Y0,
                     UI_CONFIRM_CANCEL_X1 - UI_CONFIRM_CANCEL_X0,
                     UI_CONFIRM_Y1 - UI_CONFIRM_Y0, 6, COL_PANEL_HI);
  }
  g->drawRoundRect(UI_CONFIRM_CANCEL_X0, UI_CONFIRM_Y0,
                   UI_CONFIRM_CANCEL_X1 - UI_CONFIRM_CANCEL_X0,
                   UI_CONFIRM_Y1 - UI_CONFIRM_Y0, 6, COL_BORDER);
  g->setTextSize(2);
  g->setTextColor(COL_TEXT);
  g->setCursor(UI_CONFIRM_CANCEL_X0 + 24, UI_CONFIRM_Y0 + 9);
  g->print("CANCEL");

  uint16_t okFill = pressedIn(UI_CONFIRM_OK_X0, UI_CONFIRM_Y0,
                              UI_CONFIRM_OK_X1, UI_CONFIRM_Y1) ? COL_AMBER_DIM : COL_AMBER;
  g->fillRoundRect(UI_CONFIRM_OK_X0, UI_CONFIRM_Y0,
                   UI_CONFIRM_OK_X1 - UI_CONFIRM_OK_X0,
                   UI_CONFIRM_Y1 - UI_CONFIRM_Y0, 6, okFill);
  g->setTextColor(COL_BG);
  g->setCursor(UI_CONFIRM_OK_X0 + 18, UI_CONFIRM_Y0 + 9);
  g->print("CONFIRM");
}

// Touch feedback border, overlaid on top of the fully rendered page for one
// frame every time a touch registers. Like every other element, the right
// border stops at CONTENT_RIGHT (the 20px right-edge padding) instead of
// running to the physical screen edge.
void uiDrawTouchFlash(lgfx::LovyanGFX* g) {
  g->fillRect(0, 0, CONTENT_RIGHT, 3, COL_AMBER);             // top
  g->fillRect(0, SCREEN_H - 3, CONTENT_RIGHT, 3, COL_AMBER);  // bottom
  g->fillRect(0, 0, 3, SCREEN_H, COL_AMBER);                  // left
  g->fillRect(CONTENT_RIGHT - 3, 0, 3, SCREEN_H, COL_AMBER);  // right
}
