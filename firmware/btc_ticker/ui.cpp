#include "ui.h"
#include "candles.h"
#include "settings.h"
#include <time.h>
#include <math.h>
#include <string.h>

// ── DESIGN SYSTEM ──────────────────────────────────────────
// 16-color palette (fills the 4-bit palette sprite's 16 entries exactly).
// Color is spent on signal only: green/red for market direction, amber for
// the BTC brand accent and interactive affordances, blue for the weather
// rain glyph; everything structural stays in a neutral dark-gray ramp
// (BG < GRID < BORDER < PANEL < PANEL_HI) so data always outranks chrome.
// Contrast vs panel fills is >= 4.5:1 for all text colors (WCAG AA
// on-device).
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
  0x561E,  // BLUE      #52C2F7  weather rain-icon accent (only use of blue)
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
static uint16_t COL_BLUE      = COL_BASE[15];

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
  COL_BLUE = rgb[15];
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
  COL_BLUE = 15;
}

// ── LAYOUT (4px grid; nothing renders past CONTENT_RIGHT) ──
static const int SCREEN_W      = 320;
static const int SCREEN_H      = 240;
static const int PAD_RIGHT     = 0;
static const int CONTENT_RIGHT = SCREEN_W - PAD_RIGHT;  // 320
static const int EDGE          = 4;  // standard left inset

// Every full-screen page (dashboard, Clock, Weather, HOME) reserves this
// much space at the bottom for the shared footer (see uiDrawFooter()) — its
// hairline sits right at this y. Page content must stay above it.
static const int CONTENT_BOTTOM = 212;

// Chart footprint: fixed 288px-wide plot so the candle width is always
// >= 1px even at the maximum of MAX_CANDLES visible candles. The bottom
// edge (AXIS_Y) is fixed; the top grows upward into any strip freed by
// hiding the price hero and/or the range bar so those pixels aren't wasted.
static const int CHART_W       = 288;
static const int AXIS_Y        = 190;  // x-axis hairline; labels live 196..204
static const int CONTENT_TOP   = 30;   // first content row under the status bar
static const int CONTENT_TOP_NO_STATUS = EDGE;  // status bar fully empty (no clock, no date)
// Stacked layout tops (plot is [y0, AXIS_Y)):
//   price+range → 92, price only → 72,
//   range only → 50 (or 24 if the clock and date are also both hidden),
//   neither → 30 (or 4 if the clock and date are both hidden too — see chartY0())
static const int CHART_Y0_BOTH  = 92;
static const int CHART_Y0_BOTH_NO_STATUS  = 66;
static const int CHART_Y0_PRICE = 72;
static const int CHART_Y0_PRICE_NO_STATUS = 46;
static const int CHART_Y0_RANGE = 50;
static const int CHART_Y0_RANGE_NO_STATUS = 24;
// Range-bar label/track y when stacked under the status bar (price hidden)
// vs under the price hero vs with the status bar itself reclaimed (clock and
// date both hidden). Track is vertically centered on the 8px labels.
static const int RANGE_Y_TXT_PRICE = 72;
static const int RANGE_Y_TRK_PRICE = 76;
static const int RANGE_Y_TXT_PRICE_NO_STATUS = 46;
static const int RANGE_Y_TRK_PRICE_NO_STATUS = 50;
static const int RANGE_Y_TXT_TOP   = 30;
static const int RANGE_Y_TRK_TOP   = 34;
static const int RANGE_Y_TXT_TOP_NO_STATUS = 4;
static const int RANGE_Y_TRK_TOP_NO_STATUS = 8;
// Price hero y-coordinates (see drawPriceRow), and their siblings for when
// the status bar strip is also reclaimed (clock and date both hidden) — same
// uniform 26px shift as the constants above (CONTENT_TOP - CONTENT_TOP_NO_STATUS).
static const int PRICE_DOLLAR_Y   = 50;
static const int PRICE_DOLLAR_Y_NO_STATUS = 24;
static const int PRICE_VAL_Y      = 34;
static const int PRICE_VAL_Y_NO_STATUS = 8;
static const int PRICE_CHIP_Y     = 39;
static const int PRICE_CHIP_Y_NO_STATUS = 13;
static const int PRICE_CHIP_TXT_Y = 43;
static const int PRICE_CHIP_TXT_Y_NO_STATUS = 17;

// Layout depends on the Price / Range bar toggles only (not data
// availability) so the chart doesn't jump when the first payload arrives.
// When neither claims the space, the chart also reclaims the status bar
// strip itself once it's genuinely empty (clock and date both hidden) —
// otherwise it stays under the fixed CONTENT_TOP so the date has room. The
// range-bar-only case reclaims that same strip the same way.
// True once the status bar strip itself is empty (clock and date both
// hidden) and so available to be reclaimed by whatever's stacked above it.
static bool statusReclaimed() { return !gSettings.showClock && !gSettings.showDate; }

static int chartY0() {
  bool noStatus = statusReclaimed();
  if (gSettings.showPrice && gSettings.rangeBar) {
    return noStatus ? CHART_Y0_BOTH_NO_STATUS : CHART_Y0_BOTH;
  }
  if (gSettings.showPrice) return noStatus ? CHART_Y0_PRICE_NO_STATUS : CHART_Y0_PRICE;
  if (gSettings.rangeBar) return noStatus ? CHART_Y0_RANGE_NO_STATUS : CHART_Y0_RANGE;
  return noStatus ? CONTENT_TOP_NO_STATUS : CONTENT_TOP;
}
static int chartH() { return AXIS_Y - chartY0(); }

static int rangeBarTxtY() {
  bool noStatus = statusReclaimed();
  if (gSettings.showPrice) return noStatus ? RANGE_Y_TXT_PRICE_NO_STATUS : RANGE_Y_TXT_PRICE;
  return noStatus ? RANGE_Y_TXT_TOP_NO_STATUS : RANGE_Y_TXT_TOP;
}
static int rangeBarTrkY() {
  bool noStatus = statusReclaimed();
  if (gSettings.showPrice) return noStatus ? RANGE_Y_TRK_PRICE_NO_STATUS : RANGE_Y_TRK_PRICE;
  return noStatus ? RANGE_Y_TRK_TOP_NO_STATUS : RANGE_Y_TRK_TOP;
}

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

// Horizontally centers `text` on the full screen width at row `y`, in
// `color`/`size` — the confirm and Wi-Fi-setup full-screen pages are built
// almost entirely from lines like this.
static void drawCentered(lgfx::LovyanGFX* g, int y, const char* text, uint16_t color, int size) {
  g->setTextSize(size);
  g->setTextColor(color);
  g->setCursor((SCREEN_W - g->textWidth(text)) / 2, y);
  g->print(text);
}

// Three-dot "loading" indicator, one phase per second — matches the 1Hz
// renderIfDue cadence (a faster phase would just judder since nothing forces
// extra renders while this is on screen). Shared between the boot splash and
// the chart's "no candle data yet" state; `timeMs` is whatever the caller
// wants to derive the phase from (elapsed-since-start for the splash, plain
// millis() for the chart, where there's no fixed start to measure from).
static void drawLoadingDots(lgfx::LovyanGFX* g, int cx, int cy, uint32_t timeMs) {
  int phase = (int)((timeMs / 500) % 3);
  const int spacing = 14;
  int startX = cx - spacing;
  for (int i = 0; i < 3; i++) {
    g->fillCircle(startX + i * spacing, cy, 3, i == phase ? COL_AMBER : COL_BORDER);
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

// Simple pitched-roof house glyph for the footer Home button — a filled
// triangle roof over a square body with a door notch, in the same
// hand-drawn-glyph family as drawGear()/drawWifi(). `r` is the roof's
// half-width/height budget.
static void drawHomeIcon(lgfx::LovyanGFX* g, int cx, int cy, int r, uint16_t col) {
  g->fillTriangle(cx - r, cy, cx, cy - r, cx + r, cy, col);
  int bodyW = r * 2 - 2;
  int bodyH = r;
  g->fillRect(cx - bodyW / 2, cy, bodyW, bodyH, col);
  g->fillRect(cx - 1, cy + bodyH - (r / 2 + 1), 3, r / 2 + 1, COL_BG);  // door notch
}

// Wall-calendar glyph for the HOME tile: rounded body, two hanger-ring
// tabs, and an amber "today" square — echoes the full Calendar page's
// today-highlight without drawing live date data (a static icon, like the
// Clock tile's fixed 10:10 face). `r` is the body's half-width/height budget.
static void drawCalendarIcon(lgfx::LovyanGFX* g, int cx, int cy, int r) {
  int w = r * 2 - 2, h = r * 2 - 4;
  int x0 = cx - w / 2, y0 = cy - h / 2;
  g->drawRoundRect(x0, y0, w, h, 3, COL_TEXT2);
  g->drawFastHLine(x0, y0 + 5, w, COL_TEXT2);
  g->fillRect(x0 + 3, y0 - 3, 2, 5, COL_TEXT2);
  g->fillRect(x0 + w - 5, y0 - 3, 2, 5, COL_TEXT2);
  g->fillRect(cx - 3, y0 + h - 8, 6, 5, COL_AMBER);
}

// Outlined button chrome (border + pressed-state fill), the shared shape
// behind the gear button, both confirm-page CANCEL button, the clock's
// close "X", and the Wi-Fi setup Cancel button. Callers draw their own
// label/icon on top — this only handles the border/fill boilerplate.
static void drawButtonChrome(lgfx::LovyanGFX* g, int x, int y, int w, int h,
                             int radius, bool pressed) {
  if (pressed) g->fillRoundRect(x, y, w, h, radius, COL_PANEL_HI);
  g->drawRoundRect(x, y, w, h, radius, COL_BORDER);
}

// ── STATUS BAR (y 0..24) ───────────────────────────────────
// Clock centered, date right — either can be hidden independently. Feed-
// status pulse + wifi glyph live in the footer now, next to the activity
// stats — see drawFooter(). When both clock and date are hidden the whole
// strip goes blank and chartY0() reclaims it for the chart.
static void drawStatusBar(lgfx::LovyanGFX* g, const UiState& st) {
  struct tm t;
  bool haveTime = getLocalTime(&t, 0);

  if (gSettings.showClock) {
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
    } else {
      g->setTextSize(2);
      g->setTextColor(COL_TEXT3);
      int w = g->textWidth("--:--");
      g->setCursor((SCREEN_W - w) / 2, 4);
      g->print("--:--");
    }
  }

  if (haveTime && gSettings.showDate) {
    static const char* wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mons[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char dbuf[20];
    snprintf(dbuf, sizeof(dbuf), "%s %d %s", wdays[t.tm_wday], t.tm_mday, mons[t.tm_mon]);
    g->setTextSize(1);
    g->setTextColor(COL_TEXT2);
    int dateW = g->textWidth(dbuf);
    g->setCursor(CONTENT_RIGHT - dateW, 8);
    g->print(dbuf);
  }

  if (gSettings.showClock || gSettings.showDate) {
    g->drawFastHLine(EDGE, 24, CONTENT_RIGHT - EDGE, COL_GRID);
  }
}

// ── PRICE HERO (y 30..66, or 4..40 with the status bar reclaimed) ──
// Left-aligned (reading gravity). The 24h change is a filled directional
// chip — the one element besides the price meant to read at arm's length.
static void drawPriceRow(lgfx::LovyanGFX* g, const UiState& st) {
  bool noStatus = statusReclaimed();
  int yDollar = noStatus ? PRICE_DOLLAR_Y_NO_STATUS : PRICE_DOLLAR_Y;
  int yVal = noStatus ? PRICE_VAL_Y_NO_STATUS : PRICE_VAL_Y;
  int yChip = noStatus ? PRICE_CHIP_Y_NO_STATUS : PRICE_CHIP_Y;
  int yChipTxt = noStatus ? PRICE_CHIP_TXT_Y_NO_STATUS : PRICE_CHIP_TXT_Y;

  String valStr = "--";
  if (!isnan(st.price)) valStr = fmtCommas((long)(st.price + 0.5f));

  g->setTextSize(2);
  int wDollar = g->textWidth("$");
  g->setTextSize(4);
  int wVal = g->textWidth(valStr);

  // "$" baseline-aligned with the digits.
  g->setTextSize(2);
  g->setTextColor(isnan(st.price) ? COL_TEXT3 : COL_TEXT2);
  g->setCursor(EDGE, yDollar);
  g->print("$");

  g->setTextSize(4);
  g->setTextColor(isnan(st.price) ? COL_TEXT3 : COL_TEXT);
  g->setCursor(EDGE + wDollar + 2, yVal);
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
  g->fillRoundRect(chipX, yChip, chipW, 22, 6, chipFill);
  g->setTextColor(chipText);
  g->setCursor(chipX + 8, yChipTxt);
  g->print(chipBuf);
}

// ── 24H RANGE BAR ──────────────────────────────────────────
// Full-width `low ────●──── high` track with the current price as the dot.
// Sits under the price hero when that is shown, otherwise under the status
// bar. Skipped until the first ticker payload arrives (or on a degenerate
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

  const int Y_TXT = rangeBarTxtY();
  const int Y_TRK = rangeBarTrkY();
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

// ── CHART (plot top depends on Price/Range toggles; axis strip 190..206)
static void drawChart(lgfx::LovyanGFX* g, const UiState& st) {
  const int cY0 = chartY0();
  const int cH  = chartH();

  int count = settingsVisibleCount();
  if (count < 1) count = 1;
  if (count > MAX_CANDLES) count = MAX_CANDLES;

  int x0 = EDGE;  // 4
  int plotW = CONTENT_RIGHT - 2 - x0;  // 314

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
    // Empty state (fresh boot with a wiped DB and no feed yet), instead of
    // leaving a void between the range bar and the footer. Wi-Fi up: this is
    // transient (backfill/gap-repair in flight) — a spinner says "working on
    // it" without a static label going stale. Wi-Fi down: name the actual
    // problem instead — nothing is in flight, so a spinner would be a lie.
    if (st.wifiConnected) {
      drawLoadingDots(g, x0 + plotW / 2, cY0 + cH / 2, millis());
    } else {
      const char* msg = "OFFLINE — NO CACHED DATA";
      drawCaps(g, x0 + (plotW - capsWidth(msg)) / 2, cY0 + cH / 2 - 4, msg, COL_TEXT3);
    }
    return;
  }
  float span = rangeMax - rangeMin;
  if (span < 1.0f) span = 1.0f;  // avoid div-by-zero on a dead-flat market

  auto priceToY = [&](float p) -> int {
    float f = (p - rangeMin) / span;
    int y = cY0 + cH - 1 - (int)(f * (cH - 1));
    if (y < cY0) y = cY0;
    if (y > cY0 + cH - 1) y = cY0 + cH - 1;
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
    // Solid dim area under the polyline (quad per segment = two triangles),
    // then a 2px amber line on top. A single VLine-per-sample left gaps
    // between spaced candles and read as vertical strips instead of a fill.
    const int yBot = cY0 + cH - 1;
    bool havePrev = false;
    int prevX = 0, prevY = 0;
    for (int i = 0; i < count; i++) {
      CandleRec r;
      if (!visibleCandle(i, count, haveBucket, currentBucket, r)) {
        havePrev = false;
        continue;
      }
      int x = x0 + (i * plotW + plotW / 2) / count;
      int y = priceToY(r.c);
      if (havePrev) {
        if (x == prevX) {
          int top = (prevY < y) ? prevY : y;
          if (yBot > top) g->drawFastVLine(x, top + 1, yBot - top, COL_AMBER_DIM);
        } else {
          // Polygon (prevX,prevY) → (x,y) → (x,yBot) → (prevX,yBot)
          g->fillTriangle(prevX, prevY, x, y, x, yBot, COL_AMBER_DIM);
          g->fillTriangle(prevX, prevY, x, yBot, prevX, yBot, COL_AMBER_DIM);
        }
        g->drawLine(prevX, prevY, x, y, COL_AMBER);
        g->drawLine(prevX, prevY + 1, x, y + 1, COL_AMBER);
      } else {
        // Isolated sample (first point or after a gap): stem + pixel.
        if (yBot > y) g->drawFastVLine(x, y + 1, yBot - y, COL_AMBER_DIM);
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
      int x = x0 + (i * plotW) / count;
      int nextX = x0 + ((i + 1) * plotW) / count;
      int cw = nextX - x;
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
  // centered on the chart. Anchors "where is the price now" inside the shape.
  if (!isnan(st.price)) {
    int yP = priceToY(st.price);
    if (yP < cY0 + 5) yP = cY0 + 5;
    if (yP > cY0 + cH - 7) yP = cY0 + cH - 7;
    String tag = fmtCommas((long)(st.price + 0.5f));
    int tagW = g->textWidth(tag);
    int tagBoxW = tagW + 6;
    int tagBoxX = x0 + (plotW - tagBoxW) / 2;
    // Dash left of the tag, then right of it — leave a gap for the label.
    for (int x = x0; x + 4 <= tagBoxX - 2; x += 7) {
      g->drawFastHLine(x, yP, 4, COL_AMBER_DIM);
    }
    for (int x = tagBoxX + tagBoxW + 2; x + 4 <= x0 + plotW; x += 7) {
      g->drawFastHLine(x, yP, 4, COL_AMBER_DIM);
    }
    g->fillRect(tagBoxX, yP - 5, tagBoxW, 11, COL_BG);
    g->setTextColor(COL_AMBER);
    g->setCursor(tagBoxX + 2, yP - 4);
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
    int cx = x0 + (i * plotW + plotW / 2) / count;
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

// Pulse (r=3), vertically centered on the footer row (same center as the
// gear button). Color encodes freshness (green live / amber stale / red
// offline); it blinks while connecting or while the feed is fresh. Split out
// of drawFooter() so the fast 250ms blink can be redrawn on its own — cheap,
// direct-to-panel — without paying for a full uiRender()+presentFrame() 4x
// as often (see updateFeedPulse() in btc_ticker.ino).
void uiDrawFeedPulse(lgfx::LovyanGFX* g, bool wifiConnected, uint32_t priceOkMs) {
  uint32_t now = millis();
  // Track the *slot* (COL_BASE index), not a resolved COL_* value: when `g`
  // is the palette sprite, COL_* holds palette indices (set by
  // uiUsePalette()) and fillCircle wants the index; when `g` is the raw
  // panel (the fast direct-to-panel path in updateFeedPulse(), or the
  // sprite-alloc-failed fallback), it needs a real RGB565 value instead.
  // Resolve per-call against COL_BASE/night so this works for both targets.
  int slot;
  bool pulse = false;
  if (!wifiConnected) {
    slot = 4;  // BAD
  } else if (priceOkMs == 0) {
    slot = 2;  // TEXT2
    pulse = true;
  } else {
    // Freshness is judged against the configured poll cadence: at a 5m
    // interval a 45s-old price is still "live", not "stale".
    uint32_t age = now - priceOkMs;
    uint32_t freshMs = 2 * settingsPriceIntervalMs() + 3000;
    uint32_t staleMs = 4 * settingsPriceIntervalMs() + 30000;
    if (age <= freshMs) {
      slot = 3;  // GOOD
      pulse = true;
    } else {
      slot = age <= staleMs ? 7 : 4;  // AMBER : BAD
    }
  }
  bool dotOn = !pulse || ((now / 500) % 2 == 0);  // 500ms on / 500ms off
  int finalSlot = dotOn ? slot : 0;  // 0 = BG
  uint16_t color;
  if (paletteSpr && g == static_cast<lgfx::LovyanGFX*>(paletteSpr)) {
    color = (uint16_t)finalSlot;
  } else {
    color = nightOn ? redOnly(COL_BASE[finalSlot]) : COL_BASE[finalSlot];
  }
  g->fillCircle(EDGE + 3, 226, 3, color);
}

// ── FOOTER (y 212..238) ────────────────────────────────────
// Feed-status pulse + wifi glyph + muted device stats on the left, a Home
// button on the right. Shared by every full-screen page (dashboard, Clock,
// Weather, HOME, and the top-level Settings list) — each ends its render
// with this call. The Home button (UI_FOOTER_HOME_* in ui.h) is the only way
// out of an app now — btc_ticker.ino's handleTap() checks the same hit box.
void uiDrawFooter(lgfx::LovyanGFX* g, const UiFooterStatus& fs) {
  g->drawFastHLine(EDGE, CONTENT_BOTTOM, CONTENT_RIGHT - EDGE, COL_GRID);

  uiDrawFeedPulse(g, fs.wifiConnected, fs.priceOkMs);
  // Pulse (r=3) then wifi arcs (outer r=9) with a few px of gap between them.
  const int statusY = 226;
  const int wifiX = EDGE + 3 + 3 + 4 + 9;  // 23 — clear of the pulse
  drawWifi(g, wifiX, statusY, fs.wifiConnected ? COL_GOOD : COL_BAD);

  char stats[32];
  snprintf(stats, sizeof(stats), "CPU %02u%%  RAM %02u%%  ROM %02u%%",
           (unsigned)fs.cpuPct, (unsigned)fs.ramPct, (unsigned)fs.romPct);
  g->setTextSize(1);
  g->setTextColor(COL_TEXT3);
  g->setCursor(wifiX + 9 + 6, 222);
  g->print(stats);

  bool homePressed = pressedIn(UI_FOOTER_HOME_X0, UI_FOOTER_HOME_Y0,
                               UI_FOOTER_HOME_X1, UI_FOOTER_HOME_Y1);
  drawButtonChrome(g, UI_FOOTER_HOME_X0, UI_FOOTER_HOME_Y0, UI_FOOTER_HOME_W,
                   UI_FOOTER_HOME_H, 4, homePressed);
  drawHomeIcon(g, (UI_FOOTER_HOME_X0 + UI_FOOTER_HOME_X1) / 2,
               (UI_FOOTER_HOME_Y0 + UI_FOOTER_HOME_Y1) / 2, 9,
               homePressed ? COL_AMBER : COL_TEXT2);
}

void uiRender(lgfx::LovyanGFX* g, const UiState& st) {
  g->fillScreen(COL_BG);
  drawStatusBar(g, st);
  if (gSettings.showPrice) drawPriceRow(g, st);
  drawRangeBar(g, st);
  drawChart(g, st);
  UiFooterStatus fs{st.wifiConnected, st.priceOkMs, st.cpuPct, st.romPct, st.ramPct};
  uiDrawFooter(g, fs);
}

// ── SETTINGS: MODEL ────────────────────────────────────────
static const char* const SETTINGS_ROW_LABELS[ROW_COUNT] = {
  "Brightness", "Flip screen", "Price fetch", "Candle size", "Time range", "Chart style",
  "Night schedule", "Force night mode", "Range bar", "Price", "Date", "Clock", "Forget Wi-Fi network"
};

// One-line explanation shown on each option-picker page. ROW_FORGET_AP has no
// entry that's ever shown — it skips the picker and goes straight to the
// confirmation page (see handleTap() in btc_ticker.ino), so its slot here is
// unused filler to keep the array sized ROW_COUNT.
static const char* const SETTINGS_ROW_DESC[ROW_COUNT] = {
  "Backlight level. Auto follows ambient light.",
  "Rotate the screen 180 degrees.",
  "How often Binance is polled for the price.",
  "Minutes per candle. Changing clears history.",
  "Total time shown on the chart.",
  "Candle colors, monochrome, or a line chart.",
  "Dim red-only screen from 23:00 to 08:00.",
  "Keep night mode on, ignoring the schedule.",
  "Show today's low-to-high position bar.",
  "Show the live price and 24h change.",
  "Show the date in the top status bar.",
  "Show the time in the top status bar.",
  ""
};

// Visual order of the grouped list — independent of the ROW_* enum so the
// IA can evolve without touching settings logic. Binary rows (2 options)
// toggle in place; wider rows open the picker.
//
// Grouped by scope, not by data domain: GLOBAL holds device-wide settings
// (apply no matter which app/page is on screen — brightness, orientation,
// night mode, and the Wi-Fi reset action all fit here); BTC TICKER holds
// everything that only affects that one app (polling/candle/chart config
// plus its own status-bar Clock/Date toggles — those hide/show elements of
// the *ticker's* status bar, not the full-screen CLOCK app). CLOCK has no
// configurable settings yet, so it has no section here; add one (and a
// ROW_META entry) the day it needs one.
struct SetItem { uint8_t row; const char* header; };
static const SetItem SET_ITEMS[] = {
  {0xFF, "GLOBAL"},
  {ROW_BRIGHTNESS, nullptr},
  {ROW_FLIP, nullptr},
  {ROW_NIGHT, nullptr},
  {ROW_NIGHT_FORCE, nullptr},
  {ROW_FORGET_AP, nullptr},
  {0xFF, "BTC TICKER"},
  {ROW_PRICE_IV, nullptr},
  {ROW_CANDLE_IV, nullptr},
  {ROW_RANGE, nullptr},
  {ROW_STYLE, nullptr},
  {ROW_SHOW_PRICE, nullptr},
  {ROW_RANGEBAR, nullptr},
  {ROW_SHOW_CLOCK, nullptr},
  {ROW_SHOW_DATE, nullptr},
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
  int max = total - (CONTENT_BOTTOM - UI_SET_TITLE_H);  // leaves room for the shared footer
  return max > 0 ? max : 0;
}

int uiSettingsItemAt(int scrollPx, int ty) {
  // Below CONTENT_BOTTOM is the shared footer's Home button, not a row.
  if (ty < UI_SET_TITLE_H || ty >= CONTENT_BOTTOM) return -1;
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
// Page title, left-aligned; hairline below. `showBack` draws the "< "
// affordance ahead of the title — used by the picker (back to the list) but
// not the top-level list, which exits via its footer Home button instead.
static void drawSettingsTitle(lgfx::LovyanGFX* g, const char* title, bool showBack) {
  g->setTextSize(2);
  int titleX = 8;
  if (showBack) {
    g->setTextColor(COL_AMBER);
    g->setCursor(8, 7);
    g->print("<");
    titleX = 28;
  }
  g->setTextColor(COL_TEXT);
  g->setCursor(titleX, 7);
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

void uiRenderSettings(lgfx::LovyanGFX* g, int scrollPx, const UiFooterStatus& fs) {
  g->fillScreen(COL_BG);

  // Rows scroll under the fixed title bar, above the shared footer — clip
  // so a half-scrolled row can't paint over either.
  g->setClipRect(0, UI_SET_TITLE_H, SCREEN_W, CONTENT_BOTTOM - UI_SET_TITLE_H);

  for (int i = 0; i < SET_ITEM_COUNT; i++) {
    bool isHdr = SET_ITEMS[i].header != nullptr;
    int h = isHdr ? SET_HDR_H : SET_ROW_H;
    int y = setItemTop(i) - scrollPx;
    if (y + h <= UI_SET_TITLE_H || y >= CONTENT_BOTTOM) continue;

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
    // gets the primary color. Exception: Forget Wi-Fi network is a
    // destructive action, not a value — its label carries that signal in
    // COL_BAD instead (paired with the label text itself, never color alone).
    g->setTextSize(1);
    g->setTextColor(row == ROW_FORGET_AP ? COL_BAD : COL_TEXT2);
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
    int viewH = CONTENT_BOTTOM - UI_SET_TITLE_H;
    int listH = viewH + maxScroll;
    int thumbH = viewH * viewH / listH;
    if (thumbH < 12) thumbH = 12;
    int thumbY = UI_SET_TITLE_H + (viewH - thumbH) * scrollPx / maxScroll;
    g->fillRect(CONTENT_RIGHT - 2, thumbY, 2, thumbH, COL_BORDER);
  }

  g->clearClipRect();
  drawSettingsTitle(g, "Settings", false);
  uiDrawFooter(g, fs);
}

void uiRenderSettingsPicker(lgfx::LovyanGFX* g, int row) {
  g->fillScreen(COL_BG);
  drawSettingsTitle(g, SETTINGS_ROW_LABELS[row], true);

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

  bool forgetAp = row == ROW_FORGET_AP;
  const char* title = forgetAp ? "FORGET WI-FI NETWORK?" : "CLEAR CHART HISTORY?";
  drawCentered(g, 84, title, COL_TEXT, 2);

  const char* l1 = forgetAp ? "The board will disconnect and start a" :
                              "Changing the candle size erases the";
  const char* l2 = forgetAp ? "setup hotspot for your phone to rejoin it." :
                              "stored chart history and re-downloads it.";
  drawCentered(g, 106, l1, COL_TEXT2, 1);
  drawCentered(g, 116, l2, COL_TEXT2, 1);

  // The pending change, e.g. "5m -> 15m" — not applicable to Forget Wi-Fi
  // network, which isn't a value change, so it's skipped there.
  if (!forgetAp) {
    char preview[24];
    snprintf(preview, sizeof(preview), "%s -> %s",
             settingsOptionLabel(row, settingsOptionIndex(row)),
             settingsOptionLabel(row, idx));
    drawCentered(g, 132, preview, COL_AMBER, 2);
  }

  // Buttons. Hit zones live in ui.h (UI_CONFIRM_*).
  bool cancelPressed = pressedIn(UI_CONFIRM_CANCEL_X0, UI_CONFIRM_Y0,
                                 UI_CONFIRM_CANCEL_X1, UI_CONFIRM_Y1);
  drawButtonChrome(g, UI_CONFIRM_CANCEL_X0, UI_CONFIRM_Y0,
                   UI_CONFIRM_CANCEL_X1 - UI_CONFIRM_CANCEL_X0,
                   UI_CONFIRM_Y1 - UI_CONFIRM_Y0, 6, cancelPressed);
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

// ── BOOT SPLASH ─────────────────────────────────────────────
// See ui.h for when this is shown/exited. Built from the same tokens as the
// confirm/Wi-Fi-setup full-screen pages (drawCentered/drawCaps, COL_AMBER
// accent) so it reads as part of the same design system, not a bolted-on
// screen.
void uiRenderSplash(lgfx::LovyanGFX* g, bool wifiConnected, uint32_t elapsedMs) {
  g->fillScreen(COL_BG);

  const int cx = SCREEN_W / 2;
  const int cy = 92;
  g->drawCircle(cx, cy, 30, COL_AMBER);
  g->drawCircle(cx, cy, 29, COL_AMBER);
  g->setTextSize(4);
  g->setTextColor(COL_AMBER);
  g->setCursor(cx - g->textWidth("B") / 2, cy - 16);
  g->print("B");

  const char* brand = "BTC TICKER";
  drawCaps(g, cx - capsWidth(brand) / 2, 138, brand, COL_TEXT3);

  const char* status = wifiConnected ? "Fetching market data" : "Connecting to Wi-Fi";
  drawCentered(g, 160, status, COL_TEXT2, 2);

  drawLoadingDots(g, cx, 196, elapsedMs);
}

// ── WI-FI SETUP ("FIND ACCESS MODE") ───────────────────────
// Shown after confirming Forget Wi-Fi network. Purely informational — data
// entry happens on the phone's own keyboard via the captive portal
// (wifi_portal.*), not on this resistive touch panel. Cancel reboots
// immediately; that's always safe (see wifi_creds.h) because credentials
// were already cleared here and nothing new has been saved yet.
void uiRenderWifiSetup(lgfx::LovyanGFX* g, const char* apSsid) {
  g->fillScreen(COL_BG);

  drawCentered(g, 20, "SETUP MODE", COL_AMBER, 2);

  const char* l1 = "CONNECT YOUR PHONE TO";
  drawCaps(g, (SCREEN_W - capsWidth(l1)) / 2, 64, l1, COL_TEXT3);

  drawCentered(g, 78, apSsid, COL_TEXT, 2);

  const char* l2 = "THEN OPEN";
  drawCaps(g, (SCREEN_W - capsWidth(l2)) / 2, 108, l2, COL_TEXT3);

  drawCentered(g, 122, "http://192.168.4.1", COL_TEXT, 2);

  g->drawFastHLine(EDGE, 152, CONTENT_RIGHT - EDGE, COL_GRID);
  drawCentered(g, 162, "Cancel keeps the current network.", COL_TEXT3, 1);

  // Cancel button — same outlined/pressed-fill pattern as the confirm page.
  bool pressed = pressedIn(UI_WIFI_SETUP_CANCEL_X0, UI_WIFI_SETUP_CANCEL_Y0,
                           UI_WIFI_SETUP_CANCEL_X1, UI_WIFI_SETUP_CANCEL_Y1);
  int bw = UI_WIFI_SETUP_CANCEL_X1 - UI_WIFI_SETUP_CANCEL_X0;
  int bh = UI_WIFI_SETUP_CANCEL_Y1 - UI_WIFI_SETUP_CANCEL_Y0;
  drawButtonChrome(g, UI_WIFI_SETUP_CANCEL_X0, UI_WIFI_SETUP_CANCEL_Y0, bw, bh, 6, pressed);
  g->setTextSize(2);
  g->setTextColor(COL_TEXT);
  g->setCursor(UI_WIFI_SETUP_CANCEL_X0 + 24, UI_WIFI_SETUP_CANCEL_Y0 + 9);
  g->print("CANCEL");
}

// ── FULL-SCREEN CLOCK ──────────────────────────────────────
// Minimal ambient clock built only from COL_* design tokens (BG, TEXT,
// TEXT3, AMBER, BORDER, PANEL_HI, GRID) so night mode inherits for free:
// renderIfDue calls uiSetNightMode() before uiRenderClock, which remaps
// the 4-bit palette (or the COL_* RGB565 fallbacks) to red-luminance —
// face, hands, digital digits, hairline, and close "X" all go fully red
// with no special-case code here. Three modes cycle on whole-screen tap;
// "X" top-left exits back to the dashboard.

// Angle from 12 o'clock, clockwise, in radians. Screen y grows downward so
// the y component uses -cos.
static void handTip(int cx, int cy, float angle, int len, int& ox, int& oy) {
  ox = cx + (int)(sinf(angle) * (float)len + 0.5f);
  oy = cy - (int)(cosf(angle) * (float)len + 0.5f);
}

// Rectangular hand (constant width tip-to-hub). Built as a quad from two
// triangles — no AA, no taper. Base is pulled slightly past the hub so the
// hub disc covers the join cleanly at every angle.
static void drawClockHand(lgfx::LovyanGFX* g, int cx, int cy, float angle,
                          int len, int halfW, uint16_t col) {
  int tx, ty;
  handTip(cx, cy, angle, len, tx, ty);
  // Perpendicular to the hand direction (for the rectangle's short edge).
  float px = cosf(angle);
  float py = sinf(angle);
  int bx = cx - (int)(sinf(angle) * 4.0f);
  int by = cy + (int)(cosf(angle) * 4.0f);
  int ox = (int)(px * (float)halfW + 0.5f);
  int oy = (int)(py * (float)halfW + 0.5f);
  // Quad corners: base-left, base-right, tip-right, tip-left.
  int b1x = bx + ox, b1y = by + oy;
  int b2x = bx - ox, b2y = by - oy;
  int t1x = tx + ox, t1y = ty + oy;
  int t2x = tx - ox, t2y = ty - oy;
  g->fillTriangle(b1x, b1y, b2x, b2y, t2x, t2y, col);
  g->fillTriangle(b1x, b1y, t1x, t1y, t2x, t2y, col);
}

// Thin needle with a short counterweight past the hub (classic second hand).
static void drawSecondHand(lgfx::LovyanGFX* g, int cx, int cy, float angle,
                           int len, uint16_t col) {
  int tx, ty, cx2, cy2;
  handTip(cx, cy, angle, len, tx, ty);
  // Counterweight ~18% of the forward length, opposite direction.
  handTip(cx, cy, angle + 3.14159265f, len / 5, cx2, cy2);
  g->drawLine(cx2, cy2, tx, ty, col);
}

// Draws a minimal analog face centered at (cx, cy) with outer radius `r`.
// Twelve short hour ticks mark the hours; hands are driven from `t` when
// non-null (pre-NTP the face is empty of hands). Second hand is COL_AMBER
// (orange by day, red-luminance under night mode); hour/minute use COL_TEXT.
static void drawAnalogFace(lgfx::LovyanGFX* g, int cx, int cy, int r,
                           const struct tm* t) {
  g->drawCircle(cx, cy, r, COL_TEXT);

  // 12 short hour marks just inside the rim (length scales with face size).
  // 12 o'clock is a touch longer so the top of the face is unambiguous.
  const float TAU = 6.2831853f;
  int tickOut = r - 2;
  int tickIn  = r - (r >= 70 ? 10 : 7);
  int tickIn12 = r - (r >= 70 ? 14 : 10);
  for (int h = 0; h < 12; h++) {
    float a = (float)h / 12.0f * TAU;
    int iLen = (h == 0) ? tickIn12 : tickIn;
    int x0, y0, x1, y1;
    handTip(cx, cy, a, tickOut, x0, y0);
    handTip(cx, cy, a, iLen, x1, y1);
    g->drawLine(x0, y0, x1, y1, COL_TEXT);
  }

  if (t) {
    // Continuous angles (minute contributes to hour, second to minute) so
    // hands don't jump at whole-unit boundaries.
    float sec = (float)t->tm_sec;
    float min = (float)t->tm_min + sec / 60.0f;
    float hr  = (float)(t->tm_hour % 12) + min / 60.0f;
    float aH = hr  / 12.0f * TAU;
    float aM = min / 60.0f * TAU;
    float aS = sec / 60.0f * TAU;

    drawClockHand(g, cx, cy, aH, (int)(r * 0.52f), 3, COL_TEXT);  // hour
    drawClockHand(g, cx, cy, aM, (int)(r * 0.74f), 2, COL_TEXT);  // minute
    drawSecondHand(g, cx, cy, aS, (int)(r * 0.88f), COL_AMBER);   // second
  }

  // Hub: solid disc with a tiny BG pin so hands read as attached to a pivot.
  g->fillCircle(cx, cy, 4, COL_TEXT);
  g->fillCircle(cx, cy, 1, COL_BG);
}

// 24h digital block, no seconds.
//   large (digital-only): single row "HH:MM", max size that fills the screen.
//   split pane:           "HH" over "MM" (no colon), max size for the half-box.
// GLCD glyphs are 6x8 per size unit.
static void drawDigitalBlock(lgfx::LovyanGFX* g, int x0, int y0, int w, int h,
                             const struct tm* t, bool large) {
  uint16_t col = t ? COL_TEXT : COL_TEXT3;

  if (large) {
    // Digital-only: one row HH:MM. 5 glyphs * 6 px wide, 8 px tall.
    char buf[6];
    if (t) snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    else   snprintf(buf, sizeof(buf), "--:--");

    const int padX = 4;
    const int padY = 8;
    int maxByW = (w - 2 * padX) / 30;  // 5 * 6
    int maxByH = (h - 2 * padY) / 8;
    int size = maxByW < maxByH ? maxByW : maxByH;
    if (size < 1) size = 1;

    g->setTextSize(size);
    int tw = g->textWidth(buf);
    int th = 8 * size;
    g->setTextColor(col);
    g->setCursor(x0 + (w - tw) / 2, y0 + (h - th) / 2);
    g->print(buf);
    return;
  }

  // Split pane: HH / MM stacked with breathing room (not max-fill — that
  // reads as bloated next to the analog face on a 160px half-width).
  char hh[3], mm[3];
  if (t) {
    snprintf(hh, sizeof(hh), "%02d", t->tm_hour);
    snprintf(mm, sizeof(mm), "%02d", t->tm_min);
  } else {
    snprintf(hh, sizeof(hh), "--");
    snprintf(mm, sizeof(mm), "--");
  }

  const int padX = 16;
  const int padY = 36;
  const int gap  = 14;
  const int sizeCap = 8;  // hard ceiling so digits stay balanced with the face
  int maxByW = (w - 2 * padX) / 12;          // 2 glyphs * 6 px
  int maxByH = (h - 2 * padY - gap) / 16;    // 2 rows * 8 px
  int size = maxByW < maxByH ? maxByW : maxByH;
  if (size > sizeCap) size = sizeCap;
  if (size < 1) size = 1;

  g->setTextSize(size);
  int digitW = g->textWidth(hh);
  int rowH   = 8 * size;
  int blockH = rowH * 2 + gap;
  int x = x0 + (w - digitW) / 2;
  int y = y0 + (h - blockH) / 2;

  g->setTextColor(col);
  g->setCursor(x, y);
  g->print(hh);
  g->setCursor(x, y + rowH + gap);
  g->print(mm);
}

void uiRenderClock(lgfx::LovyanGFX* g, uint8_t mode, const UiFooterStatus& fs) {
  g->fillScreen(COL_BG);

  struct tm t;
  bool haveTime = getLocalTime(&t, 0);
  const struct tm* tp = haveTime ? &t : nullptr;

  // All geometry below fits above CONTENT_BOTTOM, leaving room for the
  // shared footer — no close control to leave room for otherwise.
  if (mode == UI_CLOCK_MODE_ANALOG) {
    // Big face, vertically centered in the content area above the footer.
    drawAnalogFace(g, SCREEN_W / 2, CONTENT_BOTTOM / 2, 78, tp);
  } else if (mode == UI_CLOCK_MODE_DIGITAL) {
    drawDigitalBlock(g, 0, 0, SCREEN_W, CONTENT_BOTTOM, tp, true);
  } else {
    // Split: left half analog, right half 24h digital. Sized with margin so
    // neither pane crowds the hairline or the content-area edges.
    const int mid = SCREEN_W / 2;
    g->drawFastVLine(mid, 40, CONTENT_BOTTOM - 56, COL_GRID);
    drawAnalogFace(g, mid / 2, CONTENT_BOTTOM / 2, 68, tp);
    drawDigitalBlock(g, mid, 0, SCREEN_W - mid, CONTENT_BOTTOM, tp, false);
  }

  uiDrawFooter(g, fs);
}

// ── WEATHER ────────────────────────────────────────────────
// Built only from COL_* tokens (TEXT/TEXT2/TEXT3/BLUE/AMBER/BORDER/GRID) so
// night mode inherits for free, same as the clock page above. BLUE (the
// palette's 16th and last slot) is spent solely on the rain glyph; snow/fog
// stay in the neutral TEXT/TEXT2 grays — consistent with the rest of the UI
// spending color only on market direction (GOOD/BAD), the BTC/interactive
// accent (AMBER), and now rain (BLUE).

// Hand-drawn glyphs mapped from OWM's condition-id ranges: 2xx thunderstorm,
// 3xx-5xx drizzle/rain, 6xx snow, 800/801 clear, everything else (802-804
// clouds, 741 fog, any unmapped code) a plain cloud. `r` is the icon's
// overall half-height budget — glyph coordinates below are fractions of it
// (native reference size r=9), matching cx,cy as the icon's visual center.
// Storm/rain/snow are drawn standalone (no cloud underneath) — a cloud
// sharing the space with a tiny overlay read as clutter at these sizes.
static void drawWeatherIcon(lgfx::LovyanGFX* g, int cx, int cy, int r, uint16_t cond) {
  float s = (float)r / 9.0f;

  if (cond == 800 || cond == 801) {
    // Clear: sun disc + 8 rays (cardinals then diagonals), with a gap
    // between disc and rays so they read as separate. Plain drawLine, not
    // drawWideLine — the latter's antialiased blend reads back the
    // framebuffer, which this display's 4-bit palette sprite can't satisfy
    // (LoadProhibited crash inside LovyanGFX's palette-blend path).
    g->fillCircle(cx, cy, (int)(4 * s), COL_AMBER);
    g->drawLine(cx, cy - 9 * s, cx, cy - 7 * s, COL_AMBER);
    g->drawLine(cx, cy + 7 * s, cx, cy + 9 * s, COL_AMBER);
    g->drawLine(cx - 9 * s, cy, cx - 7 * s, cy, COL_AMBER);
    g->drawLine(cx + 7 * s, cy, cx + 9 * s, cy, COL_AMBER);
    g->drawLine(cx - 7 * s, cy - 7 * s, cx - 5 * s, cy - 5 * s, COL_AMBER);
    g->drawLine(cx + 5 * s, cy + 5 * s, cx + 7 * s, cy + 7 * s, COL_AMBER);
    g->drawLine(cx - 7 * s, cy + 7 * s, cx - 5 * s, cy + 5 * s, COL_AMBER);
    g->drawLine(cx + 5 * s, cy - 5 * s, cx + 7 * s, cy - 7 * s, COL_AMBER);
    return;
  }
  if (cond >= 200 && cond <= 232) {
    // Thunderstorm: zigzag bolt — a Material-style hexagon polygon (bottom
    // tip, notch, top prong) split into 4 fillTriangle calls since the GFX
    // API has no filled-polygon primitive.
    g->fillTriangle(cx - 5 * s, cy + 2 * s, cx + 1 * s, cy - 8 * s, cx + 1 * s, cy - 2 * s,
                     COL_AMBER);
    g->fillTriangle(cx - 5 * s, cy + 2 * s, cx + 1 * s, cy - 2 * s, cx - 1 * s, cy + 2 * s,
                     COL_AMBER);
    g->fillTriangle(cx - 1 * s, cy + 2 * s, cx + 1 * s, cy - 2 * s, cx + 5 * s, cy - 2 * s,
                     COL_AMBER);
    g->fillTriangle(cx - 1 * s, cy + 2 * s, cx + 5 * s, cy - 2 * s, cx - 1 * s, cy + 8 * s,
                     COL_AMBER);
    return;
  }
  if (cond >= 600 && cond <= 622) {
    // Snow: six-armed snowflake = three lines crossing at 60deg.
    g->drawLine(cx, cy - 7 * s, cx, cy + 7 * s, COL_TEXT);
    g->drawLine(cx - 6 * s, cy - 4 * s, cx + 6 * s, cy + 4 * s, COL_TEXT);
    g->drawLine(cx - 6 * s, cy + 4 * s, cx + 6 * s, cy - 4 * s, COL_TEXT);
    return;
  }
  if (cond >= 300 && cond <= 531) {
    // Drizzle (3xx) / rain (5xx): three staggered teardrops (triangle cap
    // fused onto a circle), two small on top, one large below.
    g->fillTriangle(cx - 6 * s, cy - 8 * s, cx - 8 * s, cy - 3 * s, cx - 4 * s, cy - 3 * s,
                     COL_BLUE);
    g->fillCircle(cx - 6 * s, cy - 3 * s, (int)(2 * s), COL_BLUE);
    g->fillTriangle(cx + 5 * s, cy - 6 * s, cx + 3 * s, cy - 1 * s, cx + 7 * s, cy - 1 * s,
                     COL_BLUE);
    g->fillCircle(cx + 5 * s, cy - 1 * s, (int)(2 * s), COL_BLUE);
    g->fillTriangle(cx - 1 * s, cy + 1 * s, cx - 4 * s, cy + 6 * s, cx + 2 * s, cy + 6 * s,
                     COL_BLUE);
    g->fillCircle(cx - 1 * s, cy + 6 * s, (int)(3 * s), COL_BLUE);
    return;
  }
  // Everything else (802-804 clouds, 741 fog, or any unmapped code) shares a
  // plain cloud: two overlapping puffs on a fully-rounded pill base.
  g->fillCircle(cx - 4 * s, cy - 2 * s, (int)(4 * s), COL_TEXT2);
  g->fillCircle(cx + 3 * s, cy - 3 * s, (int)(5 * s), COL_TEXT2);
  g->fillRoundRect(cx - 9 * s, cy - 2 * s, (int)(19 * s), (int)(9 * s), (int)(4 * s), COL_TEXT2);
}

// Shared by weatherTempWidth()/drawWeatherTemp() so the "--"-if-invalid
// formatting only lives in one place.
static void weatherFormatTemp(char* buf, size_t n, float tempC, bool valid) {
  if (valid && !isnan(tempC)) snprintf(buf, n, "%.0f", tempC);
  else snprintf(buf, n, "--");
}

// Draws a whole-degree temperature (or "--" if !valid) at text size `size`,
// with a small hollow-circle degree mark instead of relying on the GLCD
// font's extended-ASCII coverage. Returns the total pixel width drawn, so
// callers needing right-alignment or centering can measure first via
// weatherTempWidth() (same layout math, no draw).
static int weatherTempWidth(lgfx::LovyanGFX* g, int size, float tempC, bool valid) {
  g->setTextSize(size);
  char buf[8];
  weatherFormatTemp(buf, sizeof(buf), tempC, valid);
  int r = size + 1;
  return g->textWidth(buf) + r * 2 + 3;
}

static void drawWeatherTemp(lgfx::LovyanGFX* g, int x, int y, int size, float tempC,
                            uint16_t color, bool valid) {
  g->setTextSize(size);
  g->setTextColor(color);
  char buf[8];
  weatherFormatTemp(buf, sizeof(buf), tempC, valid);
  g->setCursor(x, y);
  g->print(buf);
  int r = size + 1;
  g->drawCircle(x + g->textWidth(buf) + r + 1, y + r, r, color);
}

// Local wall-clock time at the weather location (OWM's tzOffset), not the
// device's own configured TZ — gmtime() on the shifted epoch reads the
// fields as if UTC, which is exactly the location's local time.
static struct tm weatherLocalTime(uint32_t dt, int32_t tzOffset) {
  time_t t = (time_t)dt + tzOffset;
  struct tm out;
  gmtime_r(&t, &out);
  return out;
}

// iOS-widget-style forecast row bar: a full-width track with an orange
// segment from `lo` to `hi`, scaled against the shared `weekLo`/`weekHi` so
// every day's bar reads on one common scale (see drawRangeBar() above for
// the sibling single-value version used by the dashboard).
static void drawWeatherDayBar(lgfx::LovyanGFX* g, int x0, int x1, int y, float lo, float hi,
                              float weekLo, float weekHi) {
  g->drawFastHLine(x0, y, x1 - x0, COL_GRID);
  float span = weekHi - weekLo;
  if (span <= 0) return;
  int bx0 = x0 + (int)((lo - weekLo) / span * (x1 - x0));
  int bx1 = x0 + (int)((hi - weekLo) / span * (x1 - x0));
  if (bx1 <= bx0) bx1 = bx0 + 1;
  g->fillRoundRect(bx0, y - 2, bx1 - bx0, 4, 2, COL_AMBER);
}

static const char* const WEATHER_WDAY[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void uiRenderWeather(lgfx::LovyanGFX* g, const WeatherData& wx, uint32_t okMs, const char* city,
                     const UiFooterStatus& fs) {
  g->fillScreen(COL_BG);

  // ── current conditions (top) ──
  drawCaps(g, EDGE, 4, city, COL_TEXT2);

  char updBuf[24];
  if (okMs == 0) {
    snprintf(updBuf, sizeof(updBuf), "no data yet");
  } else {
    uint32_t minAgo = (millis() - okMs) / 60000UL;
    snprintf(updBuf, sizeof(updBuf), "updated %lum ago", (unsigned long)minAgo);
  }
  drawCaps(g, CONTENT_RIGHT - capsWidth(updBuf), 4, updBuf, COL_TEXT3);

  drawWeatherTemp(g, EDGE, 18, 4, wx.curTemp, COL_TEXT, wx.valid);

  // Icon + condition text (size 2) on one row, today's H/L (size 3, numbers
  // in COL_TEXT/white, "H:"/"L:" labels in COL_TEXT2/gray) on the row below.
  // Rows are right-aligned to CONTENT_RIGHT when they fit; the condition
  // row instead clamps to MIN_ROW_X and clips at CONTENT_RIGHT for the
  // longest OWM-taxonomy strings (e.g. "thunderstorm with hail"), which
  // even at size 2 can outrun the screen — better a clean cutoff than
  // overlapping curTemp.
  const int curIconR = 14;
  const int row1Y = 13;                         // icon + condition row
  const int curRowCy = row1Y + curIconR;        // icon vertical center
  const int row2Y = row1Y + curIconR * 2 + 2;   // H/L row, just under the icon
  const int MIN_ROW_X = 88;  // clears the size-4 curTemp digits + gap

  g->setTextSize(2);
  const char* desc = wx.valid ? wx.curDesc : "connecting";
  int descW = g->textWidth(desc);

  g->setTextSize(3);
  int wLbl = g->textWidth("H:");
  int hiW = weatherTempWidth(g, 3, wx.todayHi, wx.valid);
  int loW = weatherTempWidth(g, 3, wx.todayLo, wx.valid);
  int hlW = wLbl + hiW + 6 + wLbl + loW;

  int row1W = curIconR * 2 + 6 + descW;
  int rowX = CONTENT_RIGHT - row1W;
  if (rowX < MIN_ROW_X) rowX = MIN_ROW_X;
  int descTextY = curRowCy - 8;  // size-2 text is 16px tall

  drawWeatherIcon(g, rowX + curIconR, curRowCy, curIconR, wx.valid ? wx.curCond : 800);

  int descX = rowX + curIconR * 2 + 6;
  g->setClipRect(descX, descTextY, CONTENT_RIGHT - descX, 16);
  g->setTextSize(2);
  g->setTextColor(COL_TEXT2);
  g->setCursor(descX, descTextY);
  g->print(desc);
  g->clearClipRect();

  // "H:34° L:26°" built from plain "H:"/"L:" labels plus drawWeatherTemp's
  // hollow-circle degree marks (not a font glyph — see drawWeatherTemp).
  int hlX = CONTENT_RIGHT - hlW;
  g->setTextSize(3);
  g->setTextColor(COL_TEXT2);
  g->setCursor(hlX, row2Y);
  g->print("H:");
  drawWeatherTemp(g, hlX + wLbl, row2Y, 3, wx.todayHi, COL_TEXT, wx.valid);
  g->setTextSize(3);
  g->setTextColor(COL_TEXT2);
  g->setCursor(hlX + wLbl + hiW + 6, row2Y);
  g->print("L:");
  drawWeatherTemp(g, hlX + wLbl + hiW + 6 + wLbl, row2Y, 3, wx.todayLo, COL_TEXT, wx.valid);

  g->drawFastHLine(0, 68, CONTENT_RIGHT, COL_GRID);

  // ── hourly strip (next WEATHER_NUM_HOURS hours) ──
  int hourColW = SCREEN_W / WEATHER_NUM_HOURS;
  for (int i = 0; i < WEATHER_NUM_HOURS; i++) {
    int cx = i * hourColW + hourColW / 2;
    char hbuf[4] = "--";
    if (wx.valid) {
      struct tm t = weatherLocalTime(wx.hours[i].dt, wx.tzOffset);
      snprintf(hbuf, sizeof(hbuf), "%02d", t.tm_hour);
    }
    g->setTextSize(1);
    g->setTextColor(COL_TEXT3);
    g->setCursor(cx - g->textWidth(hbuf) / 2, 74);
    g->print(hbuf);

    drawWeatherIcon(g, cx, 92, 12, wx.valid ? wx.hours[i].cond : 800);

    int tw = weatherTempWidth(g, 1, wx.hours[i].temp, wx.valid);
    drawWeatherTemp(g, cx - tw / 2, 108, 1, wx.hours[i].temp, COL_TEXT, wx.valid);
  }

  g->drawFastHLine(0, 122, CONTENT_RIGHT, COL_GRID);

  // ── 5-day forecast (rows, bottom) ──
  float weekLo = 1e18f, weekHi = -1e18f;
  if (wx.valid) {
    for (int i = 0; i < WEATHER_NUM_DAYS; i++) {
      if (wx.days[i].lo < weekLo) weekLo = wx.days[i].lo;
      if (wx.days[i].hi > weekHi) weekHi = wx.days[i].hi;
    }
  }

  const int dayTop = 126;
  const int dayBottom = CONTENT_BOTTOM - 4;  // leaves room for the shared footer
  const int rowH = (dayBottom - dayTop) / WEATHER_NUM_DAYS;
  for (int i = 0; i < WEATHER_NUM_DAYS; i++) {
    int rowY = dayTop + i * rowH;
    int midY = rowY + rowH / 2;

    const char* wday = "---";
    if (wx.valid) {
      struct tm t = weatherLocalTime(wx.days[i].dt, wx.tzOffset);
      wday = WEATHER_WDAY[t.tm_wday];
    }
    drawCaps(g, EDGE, midY - 4, wday, COL_TEXT2);

    drawWeatherIcon(g, EDGE + 44, midY, 7, wx.valid ? wx.days[i].cond : 800);

    int loW = weatherTempWidth(g, 1, wx.days[i].lo, wx.valid);
    int hiW = weatherTempWidth(g, 1, wx.days[i].hi, wx.valid);
    int loX = EDGE + 64;
    int hiX = CONTENT_RIGHT - hiW;
    drawWeatherTemp(g, loX, midY - 4, 1, wx.days[i].lo, COL_TEXT3, wx.valid);
    drawWeatherTemp(g, hiX, midY - 4, 1, wx.days[i].hi, COL_TEXT, wx.valid);

    if (wx.valid) {
      drawWeatherDayBar(g, loX + loW + 8, hiX - 8, midY, wx.days[i].lo, wx.days[i].hi, weekLo,
                        weekHi);
    } else {
      g->drawFastHLine(loX + loW + 8, midY, (hiX - 8) - (loX + loW + 8), COL_GRID);
    }
  }

  uiDrawFooter(g, fs);
}

// ── CALENDAR ───────────────────────────────────────────────
// One view only: the current month off the device's synced clock, Sunday-
// first, today highlighted. No prev/next navigation and no in-page
// controls — there's no stored event data to browse to, so exit is the
// shared footer's Home button, same as Dashboard/Weather.
static const char* const CAL_MONTHS[12] = {
  "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
  "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};
static const char* const CAL_WDAY[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

static bool calIsLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int calDaysInMonth(int year, int mon /* 0-11 */) {
  static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return (mon == 1 && calIsLeapYear(year)) ? 29 : days[mon];
}

static const int CAL_COLS = 7;
static const int CAL_ROWS = 6;  // fixed, so the grid doesn't resize month to month
static const int CAL_CELL_W = 44;
static const int CAL_CELL_H = 27;
static const int CAL_GRID_TOP = 46;
static const int CAL_MARGIN_X = (SCREEN_W - CAL_COLS * CAL_CELL_W) / 2;

void uiRenderCalendar(lgfx::LovyanGFX* g, const UiFooterStatus& fs) {
  g->fillScreen(COL_BG);

  struct tm t;
  if (!getLocalTime(&t, 0)) {
    drawCentered(g, CONTENT_BOTTOM / 2 - 4, "WAITING FOR TIME SYNC", COL_TEXT3, 1);
    uiDrawFooter(g, fs);
    return;
  }

  int year = t.tm_year + 1900;
  int mon = t.tm_mon;  // 0-11

  // Weekday of the 1st of this month (0 = Sun), via a noon mktime/localtime
  // round-trip so it's clear of any DST-transition edge case.
  struct tm first = {};
  first.tm_year = t.tm_year;
  first.tm_mon = mon;
  first.tm_mday = 1;
  first.tm_hour = 12;
  time_t firstEpoch = mktime(&first);
  struct tm firstNorm;
  localtime_r(&firstEpoch, &firstNorm);
  int firstWday = firstNorm.tm_wday;
  int numDays = calDaysInMonth(year, mon);

  char header[24];
  snprintf(header, sizeof(header), "%s %d", CAL_MONTHS[mon], year);
  drawCentered(g, 8, header, COL_TEXT, 2);

  for (int c = 0; c < CAL_COLS; c++) {
    int x = CAL_MARGIN_X + c * CAL_CELL_W;
    drawCaps(g, x + (CAL_CELL_W - capsWidth(CAL_WDAY[c])) / 2, 34, CAL_WDAY[c], COL_TEXT3);
  }
  g->drawFastHLine(EDGE, 44, CONTENT_RIGHT - EDGE, COL_GRID);

  // Table box + internal cell separators, fixed at CAL_ROWS so the grid's
  // footprint doesn't shift month to month.
  int gridW = CAL_COLS * CAL_CELL_W;
  int gridH = CAL_ROWS * CAL_CELL_H;
  g->drawRect(CAL_MARGIN_X, CAL_GRID_TOP, gridW, gridH, COL_GRID);
  for (int c = 1; c < CAL_COLS; c++) {
    g->drawFastVLine(CAL_MARGIN_X + c * CAL_CELL_W, CAL_GRID_TOP, gridH, COL_GRID);
  }
  for (int r = 1; r < CAL_ROWS; r++) {
    g->drawFastHLine(CAL_MARGIN_X, CAL_GRID_TOP + r * CAL_CELL_H, gridW, COL_GRID);
  }

  for (int day = 1; day <= numDays; day++) {
    int cellIdx = firstWday + (day - 1);
    int col = cellIdx % CAL_COLS;
    int row = cellIdx / CAL_COLS;
    int x = CAL_MARGIN_X + col * CAL_CELL_W;
    int y = CAL_GRID_TOP + row * CAL_CELL_H;
    int cx = x + CAL_CELL_W / 2;
    int cy = y + CAL_CELL_H / 2;

    bool isToday = (day == t.tm_mday);
    if (isToday) g->fillCircle(cx, cy, 12, COL_AMBER);

    char buf[3];
    snprintf(buf, sizeof(buf), "%d", day);
    g->setTextSize(1);
    g->setTextColor(isToday ? COL_BG : (col == 0 || col == 6 ? COL_TEXT3 : COL_TEXT));
    g->setCursor(cx - g->textWidth(buf) / 2, cy - 4);
    g->print(buf);
  }

  uiDrawFooter(g, fs);
}

// ── HOME (iPad-style app launcher) ─────────────────────────
// 4x3 grid of square tiles, centered on screen; only the first
// UI_HOME_APP_COUNT slots are populated (BTC TICKER, CLOCK, Settings, in
// that row-major order) so future apps drop into the next free slot with
// no layout change. Geometry lives in one place (homeTileRect) shared by
// drawing and hit-testing (uiHomeTileAt) so they can't drift apart.
static const int HOME_TILE = 70;
static const int HOME_GUTTER = 6;
static const int HOME_GRID_W = UI_HOME_COLS * HOME_TILE + (UI_HOME_COLS - 1) * HOME_GUTTER;
static const int HOME_GRID_H = UI_HOME_ROWS * HOME_TILE + (UI_HOME_ROWS - 1) * HOME_GUTTER;
static const int HOME_MARGIN_X = (SCREEN_W - HOME_GRID_W) / 2;
static const int HOME_MARGIN_Y = (SCREEN_H - HOME_GRID_H) / 2;

static void homeTileRect(int slot, int& x, int& y, int& w, int& h) {
  int col = slot % UI_HOME_COLS;
  int row = slot / UI_HOME_COLS;
  x = HOME_MARGIN_X + col * (HOME_TILE + HOME_GUTTER);
  y = HOME_MARGIN_Y + row * (HOME_TILE + HOME_GUTTER);
  w = HOME_TILE;
  h = HOME_TILE;
}

int uiHomeTileAt(int x, int y) {
  for (int slot = 0; slot < UI_HOME_APP_COUNT; slot++) {
    int tx, ty, tw, th;
    homeTileRect(slot, tx, ty, tw, th);
    if (x >= tx && x < tx + tw && y >= ty && y < ty + th) return slot;
  }
  return -1;  // empty slot or gutter/margin — no-op
}

// Slot order here, in uiRenderHome()'s icon dispatch below, and in
// btc_ticker.ino's HOME_SLOT_PAGE must all agree — see the comment on
// HOME_SLOT_PAGE there.
static const char* const HOME_APP_LABELS[UI_HOME_APP_COUNT] = {
  "BTC", "CLOCK", "SETTINGS", "WEATHER", "CALENDAR"
};

void uiRenderHome(lgfx::LovyanGFX* g, const UiFooterStatus& fs) {
  g->fillScreen(COL_BG);

  for (int slot = 0; slot < UI_HOME_APP_COUNT; slot++) {
    int x, y, w, h;
    homeTileRect(slot, x, y, w, h);
    bool pressed = pressedIn(x, y, x + w, y + h);
    g->fillRoundRect(x, y, w, h, 10, pressed ? COL_PANEL_HI : COL_PANEL);
    g->drawRoundRect(x, y, w, h, 10, COL_BORDER);

    int cx = x + w / 2;
    int cy = y + h / 2 - 6;  // leave room for the caption below
    if (slot == 0) {
      // BTC TICKER: the same "B-in-circle" brand glyph as the boot splash.
      g->drawCircle(cx, cy, 16, COL_AMBER);
      g->setTextSize(2);
      g->setTextColor(COL_AMBER);
      g->setCursor(cx - g->textWidth("B") / 2, cy - 8);
      g->print("B");
    } else if (slot == 1) {
      // CLOCK: a static analog face (fixed at 10:10, the classic clock-icon
      // pose) — same component as the full-screen clock page, just at icon
      // radius. Deliberately not live: it's an app icon, not a second clock,
      // so it shouldn't tick every render.
      struct tm iconTime = {};
      iconTime.tm_hour = 10;
      iconTime.tm_min = 10;
      iconTime.tm_sec = 0;
      drawAnalogFace(g, cx, cy, 18, &iconTime);
    } else if (slot == 2) {
      // Settings: the same gear glyph the old footer button used.
      drawGear(g, cx, cy, COL_TEXT2);
    } else if (slot == 3) {
      // Weather: sun glyph regardless of live conditions — a fixed condition
      // id here would go stale before the first fetch completes.
      drawWeatherIcon(g, cx, cy, 16, 800);
    } else {
      // Calendar: wall-calendar glyph — rounded body, two hanger rings, and
      // an amber "today" square, echoing the full-page grid's highlight.
      drawCalendarIcon(g, cx, cy, 16);
    }

    const char* caption = HOME_APP_LABELS[slot];
    drawCaps(g, x + (w - capsWidth(caption)) / 2, y + h - 14, caption, COL_TEXT2);
  }

  uiDrawFooter(g, fs);
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
