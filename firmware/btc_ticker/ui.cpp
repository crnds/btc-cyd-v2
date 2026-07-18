#include "ui.h"
#include "candles.h"
#include "settings.h"
#include <time.h>
#include <math.h>

// RGB565 defaults; uiUsePalette() remaps these to palette indices when the
// frame buffer is a palette sprite, so every draw call below works unchanged
// in both modes (palette index vs direct-to-panel RGB565).
// COL_BASE order doubles as the palette index order.
static const uint16_t COL_BASE[] = {
  0x0000,  // BG
  0xFFFF,  // TEXT
  0x9CD3,  // TEXT2
  0x2668,  // GOOD bright green
  0xF8C6,  // BAD bright red/rose
  0x1462,  // GOOD_DIM dim green (wick)
  0x9124,  // BAD_DIM dim red (wick)
  0xFDA0,  // AMBER
  0x39C7,  // BORDER
  0x8410,  // BW_BULL_DIM mid-gray, B/W style at 1-2px widths
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
  COL_BG = rgb[0];       COL_TEXT = rgb[1];    COL_TEXT2 = rgb[2];
  COL_GOOD = rgb[3];     COL_BAD = rgb[4];     COL_GOOD_DIM = rgb[5];
  COL_BAD_DIM = rgb[6];  COL_AMBER = rgb[7];   COL_BORDER = rgb[8];
  COL_BW_BULL_DIM = rgb[9];
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
  COL_BG = 0;       COL_TEXT = 1;    COL_TEXT2 = 2;  COL_GOOD = 3;
  COL_BAD = 4;      COL_GOOD_DIM = 5; COL_BAD_DIM = 6; COL_AMBER = 7;
  COL_BORDER = 8;   COL_BW_BULL_DIM = 9;
}

static const int SCREEN_W      = 320;
static const int SCREEN_H      = 240;
static const int PAD_RIGHT     = 20;
static const int CONTENT_RIGHT = SCREEN_W - PAD_RIGHT;  // 300

static const int CHART_W  = 288;  // fixed on-screen footprint, independent of candle count
static const int CHART_Y0 = 84;
static const int CHART_H  = 128;  // y 84..212

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

static void drawWifi(lgfx::LovyanGFX* g, int32_t x, int32_t y, uint16_t color) {
  g->fillCircle(x, y, 1, color);
  g->drawArc(x, y, 5, 5, 225.0f, 315.0f, color);
  g->drawArc(x, y, 9, 9, 225.0f, 315.0f, color);
  g->drawArc(x, y, 13, 13, 225.0f, 315.0f, color);
}

static void drawGear(lgfx::LovyanGFX* g, int cx, int cy, uint16_t col) {
  static const int8_t tx[8] = {8, 6, 0, -6, -8, -6, 0, 6};
  static const int8_t ty[8] = {0, 6, 8, 6, 0, -6, -8, -6};
  for (int i = 0; i < 8; i++) g->fillRect(cx + tx[i] - 1, cy + ty[i] - 1, 3, 3, col);
  g->fillCircle(cx, cy, 6, col);
  g->fillCircle(cx, cy, 2, COL_BG);  // hub hole
}

static void drawHeader(lgfx::LovyanGFX* g, const UiState& st) {
  struct tm t;
  bool haveTime = getLocalTime(&t, 0);

  g->setTextSize(2);
  g->setCursor(4, 4);
  if (haveTime) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    g->setTextColor(COL_TEXT);
    g->print(buf);

    char sbuf[4];
    snprintf(sbuf, sizeof(sbuf), ":%02d", t.tm_sec);
    g->setTextSize(1);
    g->setTextColor(COL_TEXT2);
    g->setCursor(g->getCursorX(), 10);
    g->print(sbuf);

    static const char* wdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* mons[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char dbuf[20];
    snprintf(dbuf, sizeof(dbuf), "%s %d %s", wdays[t.tm_wday], t.tm_mday, mons[t.tm_mon]);
    g->setTextColor(COL_TEXT2);
    g->setTextSize(1);
    g->setCursor(100, 10);
    g->print(dbuf);
  } else {
    g->setTextColor(COL_TEXT2);
    g->print("--:--:--");
  }

  // status dots, right-aligned — shifted left to leave room for the gear
  // icon, which itself sits flush against CONTENT_RIGHT (the same 20px
  // right-edge padding every other element on this screen respects)
  uint32_t now = millis();
  drawWifi(g, CONTENT_RIGHT - 35, 20, st.wifiConnected ? COL_GOOD : COL_BAD);

  uint16_t freshColor = COL_BAD;
  if (st.priceOkMs != 0) {
    uint32_t age = now - st.priceOkMs;
    if (age <= 5000) freshColor = COL_GOOD;
    else if (age <= 30000) freshColor = COL_AMBER;
  }
  g->fillCircle(CONTENT_RIGHT - 59, 12, 4, freshColor);

  drawGear(g, CONTENT_RIGHT - 9, 16, COL_TEXT2);
}

static void drawPriceRow(lgfx::LovyanGFX* g, const UiState& st) {
  String priceStr = "$--";
  if (!isnan(st.price)) {
    priceStr = "$" + fmtCommas((long)(st.price + 0.5f));
  }

  char changeBuf[12] = "--";
  if (!isnan(st.changePct)) {
    snprintf(changeBuf, sizeof(changeBuf), "%+.2f%%", st.changePct);
  }
  String changeStr(changeBuf);

  // Width calculations
  g->setTextSize(4);
  int wPrice = g->textWidth(priceStr);
  g->setTextSize(2);
  int wChange = g->textWidth(changeStr);

  int gap = 8;
  int totalW = wPrice + gap + wChange;

  // Start X position to right-align the combined block
  int startX = CONTENT_RIGHT - totalW;
  if (startX < 4) startX = 4;

  // Render Price (size 4, y=34)
  g->setTextSize(4);
  g->setTextColor(isnan(st.price) ? COL_TEXT2 : COL_TEXT);
  g->setCursor(startX, 34);
  g->print(priceStr);

  // Render Percentage Change (size 2, inline to the right, y=50 for baseline alignment)
  g->setTextSize(2);
  g->setTextColor(isnan(st.changePct) ? COL_TEXT2 : (st.changePct >= 0 ? COL_GOOD : COL_BAD));
  g->setCursor(startX + wPrice + gap, 50);
  g->print(changeStr);
}

static void drawChart(lgfx::LovyanGFX* g) {
int count = settingsVisibleCount();
if (count < 1) count = 1;
if (count > MAX_CANDLES) count = MAX_CANDLES;
int cw = CHART_W / count;
if (cw < 1) cw = 1;
int plotW = cw * count;
int x0 = CONTENT_RIGHT - 2 - plotW;  // right edge lands on CONTENT_RIGHT

g->drawRect(x0 - 2, CHART_Y0 - 2, plotW + 4, CHART_H + 4, COL_BORDER);

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
if (!haveRange) return;
float span = rangeMax - rangeMin;
if (span < 1.0f) span = 1.0f;  // avoid div-by-zero on a dead-flat market

auto priceToY = [&](float p) -> int {
float f = (p - rangeMin) / span;
int y = CHART_Y0 + CHART_H - 1 - (int)(f * (CHART_H - 1));
if (y < CHART_Y0) y = CHART_Y0;
if (y > CHART_Y0 + CHART_H - 1) y = CHART_Y0 + CHART_H - 1;
return y;
};

ChartStyle style = (ChartStyle)gSettings.styleIdx;

if (style == STYLE_LINE) {
bool havePrev = false;
int prevX = 0, prevY = 0;
for (int i = 0; i < count; i++) {
CandleRec r;
if (!visibleCandle(i, count, haveBucket, currentBucket, r)) { havePrev = false; continue; }
int x = x0 + i * cw + cw / 2;
int y = priceToY(r.c);
if (havePrev) g->drawLine(prevX, prevY, x, y, COL_AMBER);
else g->drawPixel(x, y, COL_AMBER);
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
g->fillRect(x, yTop, bodyW, bodyLen, bull ? COL_GOOD : COL_BAD);
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

g->setTextColor(COL_TEXT2);
g->setTextSize(1);
g->setCursor(x0 + 2, CHART_Y0 + 1);
g->print(fmtCommas((long)(rangeMax + 0.5f)));
g->setCursor(x0 + 2, CHART_Y0 + CHART_H - 9);
g->print(fmtCommas((long)(rangeMin + 0.5f)));
}

static void drawFooter(lgfx::LovyanGFX* g, const UiState& st) {
g->setTextSize(1);

int x = 4;
const int y = 222;
auto drawStat = [&](const char* label, uint8_t pct) {
g->setTextColor(COL_TEXT2);
g->setCursor(x, y);
g->print(label);
x = g->getCursorX();

char buf[5];
snprintf(buf, sizeof(buf), "%u", (unsigned)pct);
g->setTextColor(COL_TEXT);
g->setCursor(x, y);
g->print(buf);
x = g->getCursorX();

g->setTextColor(COL_TEXT2);
g->setCursor(x, y);
g->print("%  ");
x = g->getCursorX();
};

drawStat("CPU: ", st.cpuPct);
drawStat("ROM: ", st.romPct);
drawStat("RAM: ", st.ramPct);
}

void uiRender(lgfx::LovyanGFX* g, const UiState& st) {
g->fillScreen(COL_BG);
drawHeader(g, st);
drawPriceRow(g, st);
drawChart(g);
drawFooter(g, st);
}

static const char* const SETTINGS_ROW_LABELS[ROW_COUNT] = {
  "Brightness", "Flip screen", "Price fetch", "Candle size", "Time range", "Chart style",
  "Night schedule", "Night mode active"
};

// Fixed title bar shared by the settings list and the option picker.
static void drawSettingsTitle(lgfx::LovyanGFX* g, const char* title) {
  g->setTextSize(2);
  g->setTextColor(COL_TEXT);
  g->setCursor(8, 8);
  g->print("< Back");

  int tw = g->textWidth(title);
  g->setCursor(CONTENT_RIGHT - 8 - tw, 8);
  g->print(title);

  g->drawFastHLine(0, UI_SET_TITLE_H - 1, CONTENT_RIGHT, COL_BORDER);
}

void uiRenderSettings(lgfx::LovyanGFX* g, int scrollPx) {
  g->fillScreen(COL_BG);

  // Rows scroll under the fixed title bar — clip so a half-scrolled row
  // can't paint over it.
  g->setClipRect(0, UI_SET_TITLE_H, SCREEN_W, SCREEN_H - UI_SET_TITLE_H);

  for (int i = 0; i < ROW_COUNT; i++) {
    int y = UI_SET_TITLE_H + i * UI_SET_ROW_H - scrollPx;
    if (y + UI_SET_ROW_H <= UI_SET_TITLE_H || y >= SCREEN_H) continue;

    g->setTextSize(1);
    g->setTextColor(COL_TEXT);
    g->setCursor(12, y + (UI_SET_ROW_H - 8) / 2);
    g->print(SETTINGS_ROW_LABELS[i]);

    g->setTextSize(2);
    int chevX = CONTENT_RIGHT - 10 - g->textWidth(">");
    g->setTextColor(COL_TEXT2);
    g->setCursor(chevX, y + (UI_SET_ROW_H - 16) / 2);
    g->print(">");

    const char* val = settingsValueLabel(i);
    int vw = g->textWidth(val);
    g->setTextColor(COL_AMBER);
    g->setCursor(chevX - 8 - vw, y + (UI_SET_ROW_H - 16) / 2);
    g->print(val);

    g->drawFastHLine(0, y + UI_SET_ROW_H - 1, CONTENT_RIGHT, COL_BORDER);
  }

  // Scrollbar thumb, flush against CONTENT_RIGHT.
  int listH = ROW_COUNT * UI_SET_ROW_H;
  if (listH > UI_SET_VIEW_H) {
    int thumbH = UI_SET_VIEW_H * UI_SET_VIEW_H / listH;
    if (thumbH < 12) thumbH = 12;
    int maxScroll = listH - UI_SET_VIEW_H;
    int thumbY = UI_SET_TITLE_H + (UI_SET_VIEW_H - thumbH) * scrollPx / maxScroll;
    g->fillRect(CONTENT_RIGHT - 3, thumbY, 3, thumbH, COL_TEXT2);
  }

  g->clearClipRect();
  drawSettingsTitle(g, "Settings");
}

void uiRenderSettingsPicker(lgfx::LovyanGFX* g, int row) {
  g->fillScreen(COL_BG);
  drawSettingsTitle(g, SETTINGS_ROW_LABELS[row]);

  int n = settingsOptionCount(row);
  int cur = settingsOptionIndex(row);
  for (int i = 0; i < n; i++) {
    int y = UI_SET_TITLE_H + i * UI_PICK_ROW_H;
    int cy = y + UI_PICK_ROW_H / 2;
    bool sel = i == cur;

    g->drawCircle(18, cy, 6, sel ? COL_AMBER : COL_TEXT2);
    if (sel) g->fillCircle(18, cy, 3, COL_AMBER);

    g->setTextSize(2);
    g->setTextColor(sel ? COL_TEXT : COL_TEXT2);
    g->setCursor(36, cy - 8);
    g->print(settingsOptionLabel(row, i));

    g->drawFastHLine(0, y + UI_PICK_ROW_H - 1, CONTENT_RIGHT, COL_BORDER);
  }
}

