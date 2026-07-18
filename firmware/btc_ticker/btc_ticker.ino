// BTC/USDT ticker + clock + candlestick dashboard for the CYD board
// (ESP32-2432S028R). Price from Binance REST, up to 288 candles with a
// LittleFS ring-file mini-database surviving reboots/power loss. Candle
// size, chart range, price fetch cadence, chart style, brightness, night
// mode (red-only UI 23:00-08:00, plus a manual force-on), and screen
// orientation are all user-configurable from the Settings page (gear icon,
// top right).

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <esp_system.h>
#include <math.h>

#include "pins.h"
#include "config.h"
#include "candles.h"
#include "store.h"
#include "net_http.h"
#include "net_price.h"
#include "net_klines.h"
#include "settings.h"
#include "ui.h"

// ── DISPLAY CONFIG (verbatim from cyd-stripdown/firmware/cyd_dashboard) ──
class LGFX : public lgfx::LGFX_Device {
lgfx::Panel_ILI9341 _panel_instance;
lgfx::Bus_SPI _bus_instance;
lgfx::Light_PWM _light_instance;
lgfx::Touch_XPT2046 _touch_instance;

public:
LGFX(void) {
{
auto cfg = _bus_instance.config();
cfg.spi_host = VSPI_HOST;
cfg.spi_mode = 0;
cfg.freq_write = 40000000;
cfg.freq_read = 16000000;
cfg.pin_sclk = CYD_TFT_SCLK;
cfg.pin_mosi = CYD_TFT_MOSI;
cfg.pin_miso = CYD_TFT_MISO;
cfg.pin_dc = CYD_TFT_DC;
_bus_instance.config(cfg);
_panel_instance.setBus(&_bus_instance);
}
{
auto cfg = _panel_instance.config();
cfg.pin_cs = CYD_TFT_CS;
cfg.pin_rst = CYD_TFT_RST;
cfg.panel_width = 240;
cfg.panel_height = 320;
cfg.offset_rotation = 0;
_panel_instance.config(cfg);
}
{
auto cfg = _light_instance.config();
cfg.pin_bl = CYD_TFT_BL;
cfg.invert = false;
_light_instance.config(cfg);
_panel_instance.setLight(&_light_instance);
}
{
auto cfg = _touch_instance.config();
// This panel is native portrait (240x320) and rotation=1 turns it landscape
// by swapping the raw touch axes internally — so cfg.y_min/y_max is what
// actually governs the screen's X axis here, not cfg.x_min/x_max. Confirmed
// on-device: with both pairs in raw ADC order, physical left/right were
// mirrored (screen-X inverted) while screen-Y was correct. y_min/y_max
// swapped vs raw ADC order to invert just that axis.
cfg.x_min = 200;
cfg.x_max = 3800;
cfg.y_min = 3800;
cfg.y_max = 200;
cfg.spi_host = -1;
cfg.bus_shared = false;
cfg.pin_sclk = CYD_TOUCH_SCLK;
cfg.pin_mosi = CYD_TOUCH_MOSI;
cfg.pin_miso = CYD_TOUCH_MISO;
cfg.pin_cs = CYD_TOUCH_CS;
cfg.pin_int = CYD_TOUCH_IRQ;
cfg.freq = 1000000;
_touch_instance.config(cfg);
_panel_instance.setTouch(&_touch_instance);
}
setPanel(&_panel_instance);
}
};

LGFX gfx;
LGFX_Sprite frame(&gfx);
lgfx::LovyanGFX* g = &gfx;

void presentFrame() {
if (g == &frame) frame.pushSprite(0, 0);
}

// ── STATE ──────────────────────────────────────────────────
static const char* TZ_INFO = "ICT-7";       // POSIX TZ: Asia/Bangkok, UTC+7
static const char* NTP_SERVER = "pool.ntp.org";
static const int DAILY_RESTART_HOUR = 4;

static const uint32_t TOUCH_DEBOUNCE_MS = 350;

Preferences prefs;
bool touchWasDown = false;
uint32_t lastTouchMs = 0;

float lastPrice = NAN;
float changePct = NAN;
uint32_t priceOkMs = 0;

enum class Page : uint8_t { DASHBOARD, SETTINGS };
static Page currentPage = Page::DASHBOARD;
static int settingsScroll = 0;  // px, 0..(list height - viewport)
static int pickerRow = -1;      // -1 = settings list, >=0 = option picker for that row

static void renderIfDue(bool force = false);
static void applySettingChange(uint8_t mask);

// ── touch ──────────────────────────────────────────────────
// Dashboard: gear opens Settings. Settings list: drag scrolls, tap on a row
// opens its option picker; picker: tap an option to select it and return.
// Taps fire on release so a drag that starts on a row doesn't also select it.
static int pressX = 0, pressY = 0;
static bool dragging = false;
static int dragStartScroll = 0;
static const int DRAG_THRESHOLD_PX = 10;  // resistive touch jitters a few px

static void handleTap(int tx, int ty) {
if (currentPage == Page::DASHBOARD) {
if (tx >= UI_GEAR_HIT_X0 && ty <= UI_GEAR_HIT_Y1) {
currentPage = Page::SETTINGS;
settingsScroll = 0;
pickerRow = -1;
renderIfDue(true);
}
return;
}

// Option picker page
if (pickerRow >= 0) {
if (ty < UI_SET_TITLE_H) {
if (tx < UI_BACK_HIT_X1) {
pickerRow = -1;
renderIfDue(true);
}
return;
}
int idx = (ty - UI_SET_TITLE_H) / UI_PICK_ROW_H;
if (idx < settingsOptionCount(pickerRow)) {
uint8_t mask = settingsSet(pickerRow, (uint8_t)idx);
for (int r = 0; r < ROW_COUNT; r++) {
if (mask & (1u << r)) settingsSaveRow(prefs, r);
}
applySettingChange(mask);
pickerRow = -1;  // back to the list, which now shows the new value
renderIfDue(true);
}
return;
}

// Settings list page
if (ty < UI_SET_TITLE_H) {
if (tx < UI_BACK_HIT_X1) {
currentPage = Page::DASHBOARD;
renderIfDue(true);
}
return;
}
int row = (ty - UI_SET_TITLE_H + settingsScroll) / UI_SET_ROW_H;
if (row < ROW_COUNT) {
pickerRow = row;
renderIfDue(true);
}
}

void handleTouch() {
int32_t tx, ty;
bool touchDown = gfx.getTouch(&tx, &ty);
uint32_t now = millis();

if (touchDown && !touchWasDown) {
// press: remember where, in case this becomes a drag
pressX = tx;
pressY = ty;
dragging = false;
dragStartScroll = settingsScroll;
} else if (touchDown && touchWasDown) {
// move: only the settings list scrolls
if (currentPage == Page::SETTINGS && pickerRow < 0) {
int dy = (int)ty - pressY;
if (dragging || abs(dy) > DRAG_THRESHOLD_PX) {
dragging = true;
int maxScroll = ROW_COUNT * UI_SET_ROW_H - UI_SET_VIEW_H;
if (maxScroll < 0) maxScroll = 0;
int s = dragStartScroll - dy;
if (s < 0) s = 0;
if (s > maxScroll) s = maxScroll;
if (s != settingsScroll) {
settingsScroll = s;
renderIfDue(true);
}
}
}
} else if (!touchDown && touchWasDown) {
// release: a tap unless it turned into a drag
if (!dragging && now - lastTouchMs > TOUCH_DEBOUNCE_MS) {
lastTouchMs = now;
handleTap(pressX, pressY);
}
dragging = false;
}
touchWasDown = touchDown;
}

// ── fetch jobs ────────────────────────────────────────────
static bool jobPrice() {
float p, c;
if (!fetchPrice(p, c)) return false;
lastPrice = p;
if (!isnan(c)) changePct = c;
priceOkMs = millis();
// At 1m/5m fetch cadence the keep-alive socket sits idle (likely dead)
// between polls anyway — release its ~45KB mbedTLS session heap.
if (settingsPriceIntervalMs() >= 60000) fetchPriceRelease();
return true;
}

static bool jobNtp() {
configTzTime(TZ_INFO, NTP_SERVER);
return true;
}

struct Job {
const char* name;
uint32_t interval;
bool (*run)();
uint32_t next;
uint32_t backoff;
};

static const uint32_t RETRY_BASE_MS = 1000;
static const uint32_t RETRY_MAX_MS = 60000;
static const uint32_t PRICE_INTERVAL_MS = 1000;  // overwritten from settings in setup()
static const uint32_t NTP_INTERVAL_MS = 6UL * 3600 * 1000;

static Job jobs[] = {
{"price", PRICE_INTERVAL_MS, jobPrice, 0, RETRY_BASE_MS},
{"ntp", NTP_INTERVAL_MS, jobNtp, 0, RETRY_BASE_MS},
};

// runs at most ONE due job per pass, round-robin, so a high-frequency job
// (price) can't starve a slow one (ntp) — see btcticker-cyd/src/main.cpp
static void serviceJobs() {
if (WiFi.status() != WL_CONNECTED) return;
uint32_t now = millis();
static int rrStart = 0;
const int N = sizeof(jobs) / sizeof(jobs[0]);
for (int i = 0; i < N; i++) {
int idx = (rrStart + i) % N;
Job& j = jobs[idx];
if ((int32_t)(now - j.next) < 0) continue;
bool ok = j.run();
uint32_t done = millis();
if (ok) {
j.backoff = RETRY_BASE_MS;
j.next = done + j.interval;
} else {
Serial.printf("[%s] job failed, retry in %lus\n", j.name,
(unsigned long)(j.backoff / 1000));
j.next = done + j.backoff;
j.backoff = j.backoff * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : j.backoff * 2;
}
rrStart = (idx + 1) % N;
break;
}
}

// ── backfill / gap-repair (on-demand, not fixed-interval) ─
static bool backfillDoneOnce = false;
static uint32_t backfillNextMs = 0;
static uint32_t backfillBackoff = RETRY_BASE_MS;

static void maybeBackfill() {
if (WiFi.status() != WL_CONNECTED) return;
if (time(nullptr) < NTP_VALID_EPOCH) return;

int gap = 0;
bool needGap = candlesNeedGapRepair(gap);
if (backfillDoneOnce && !needGap) return;

uint32_t nowMs = millis();
if ((int32_t)(nowMs - backfillNextMs) < 0) return;

int wantCount = settingsVisibleCount();
int limit = backfillDoneOnce ? min(wantCount, gap + 1) : wantCount;
// klines backfill needs its own TLS handshake headroom; price's session
// sits on ~45KB indefinitely once open, which is enough to starve a
// second concurrent mbedTLS context on this board's tight heap.
fetchPriceRelease();
// staging buffer is heap-transient (~5.7KB, only alive during the fetch)
// rather than static or stack (loop task has only an 8KB stack)
CandleRec* buf = (CandleRec*)malloc(sizeof(CandleRec) * MAX_CANDLES);
int n = buf ? klinesFetch(settingsBinanceInterval(), limit, buf, MAX_CANDLES) : 0;
if (n < 2) {
free(buf);
Serial.printf("backfill failed (limit=%d), retry in %lus\n", limit,
(unsigned long)(backfillBackoff / 1000));
backfillNextMs = nowMs + backfillBackoff;
backfillBackoff = backfillBackoff * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : backfillBackoff * 2;
return;
}
for (int i = 0; i < n - 1; i++) candlesSetClosed(buf[i]);
candlesSeedForming(buf[n - 1]);
free(buf);
Serial.printf("backfill: %d candles (limit=%d)\n", n, limit);
backfillDoneOnce = true;
backfillBackoff = RETRY_BASE_MS;
backfillNextMs = nowMs;
}

// ── apply a settings change live (called right after settingsCycle) ──
static void applySettingChange(uint8_t mask) {
// Brightness itself is applied by applyNightBrightness (called every
// renderIfDue right after this), which also accounts for night-mode dim —
// setting it here too would flash the user's chosen brightness on screen
// for one frame even while night mode is forcing 5%.
if (mask & (1u << ROW_FLIP)) {
gfx.setRotation(gSettings.flip ? 3 : 1);
}
if (mask & (1u << ROW_PRICE_IV)) {
jobs[0].interval = settingsPriceIntervalMs();
jobs[0].next = millis();
}
if (mask & (1u << ROW_CANDLE_IV)) {
// Slot assignments are meaningless under a new bucket width — wipe and
// re-backfill from scratch.
candlesReset(settingsCandleSeconds());
backfillDoneOnce = false;
backfillBackoff = RETRY_BASE_MS;
backfillNextMs = millis();
}
if (mask & (1u << ROW_RANGE)) {
// Ring/store are interval-keyed, not range-keyed — a range change just
// needs a refetch of the (possibly wider) visible window, no wipe.
backfillDoneOnce = false;
backfillNextMs = millis();
}
// ROW_STYLE, ROW_NIGHT and ROW_NIGHT_FORCE need no side effect — the next
// render reads gSettings / applies uiSetNightMode directly.
}

// ── WiFi supervisor ────────────────────────────────────────
static uint32_t wifiDownSinceMs = 0;
static uint32_t wifiLastRetryMs = 0;

static void wifiSupervisor() {
uint32_t now = millis();
if (WiFi.status() == WL_CONNECTED) {
wifiDownSinceMs = 0;
return;
}
if (wifiDownSinceMs == 0) wifiDownSinceMs = now;
uint32_t downFor = now - wifiDownSinceMs;
if (downFor > 10UL * 60 * 1000) {
Serial.println("wifi down >10min, restarting");
esp_restart();
} else if (downFor > 60000UL && now - wifiLastRetryMs > 60000UL) {
wifiLastRetryMs = now;
Serial.println("wifi down >60s, re-begin()");
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
}

// ── daily restart / heap log ───────────────────────────────
static void maybeDailyRestart() {
if (millis() < 3600000UL) return;
struct tm t;
if (getLocalTime(&t, 0) && t.tm_hour == DAILY_RESTART_HOUR) {
Serial.println("daily restart");
esp_restart();
}
}

static void maybeHeapLog() {
static uint32_t last = 0;
uint32_t now = millis();
if (now - last < 60000UL) return;
last = now;
Serial.printf("heap: %u free\n", (unsigned)ESP.getFreeHeap());
}

// ── device status stats (footer) ───────────────────────────
static uint8_t gCpuPct = 0;  // EMA of loop busy-time, updated each loop()

static uint8_t romUsagePct() {
uint32_t used = ESP.getSketchSize();
uint32_t total = used + ESP.getFreeSketchSpace();
return total ? (uint8_t)((used * 100UL) / total) : 0;
}

static uint8_t ramUsagePct() {
uint32_t total = ESP.getHeapSize();
return total ? (uint8_t)(((total - ESP.getFreeHeap()) * 100UL) / total) : 0;
}

// ── render (1Hz, or immediately after any accepted touch) ──
// Night mode: forced on by "Night mode active", else red-only UI between
// 23:00 and 08:00 local when "Night schedule" is enabled.
// Pre-NTP (no valid clock) keeps normal colors.
static bool nightModeActive() {
if (gSettings.nightForce) return true;
if (!gSettings.nightEn) return false;
struct tm t;
if (!getLocalTime(&t, 0)) return false;
return t.tm_hour >= 23 || t.tm_hour < 8;
}

// Dims to 5% brightness while night mode is active, restoring the user's
// chosen brightness setting the rest of the time. Unconditional (no
// on/off-edge memo) so a brightness-setting change made *while* night mode
// is active can't desync from what's actually on the panel.
static void applyNightBrightness(bool on) {
gfx.setBrightness(on ? BRI_VAL[0] : BRI_VAL[gSettings.briIdx]);
}

static void renderIfDue(bool force) {
static uint32_t lastRenderMs = 0;
static bool renderedOnce = false;
uint32_t now = millis();
if (!force && renderedOnce && now - lastRenderMs < 1000) return;
lastRenderMs = now;
renderedOnce = true;

bool night = nightModeActive();
uiSetNightMode(night);
applyNightBrightness(night);
if (currentPage == Page::SETTINGS) {
  if (pickerRow >= 0) uiRenderSettingsPicker(g, pickerRow);
  else uiRenderSettings(g, settingsScroll);
} else {
UiState st;
st.wifiConnected = WiFi.status() == WL_CONNECTED;
st.priceOkMs = priceOkMs;
st.price = lastPrice;
st.changePct = changePct;
st.cpuPct = gCpuPct;
st.romPct = romUsagePct();
st.ramPct = ramUsagePct();
uiRender(g, st);
}
presentFrame();
}

// ── SETUP / LOOP ───────────────────────────────────────────
void setup() {
Serial.begin(115200);
gfx.init();

// 4-bit palette sprite: 320*240/2 = 38,400 bytes (vs 153,600 at 16-bit).
// The UI draws with exactly 10 colors, so the palette reproduces the 16-bit
// output pixel-for-pixel; LovyanGFX converts palette->RGB565 on pushSprite.
// Allocated before WiFi.begin() so the block is contiguous on a fresh heap.
frame.setColorDepth(lgfx::palette_4bit);
if (frame.createSprite(320, 240)) {
g = &frame;
uiUsePalette(frame);
Serial.println("[frame] using 4-bit palette frame buffer");
} else {
g = &gfx;
Serial.println("[frame] sprite alloc FAILED, drawing direct (will flicker)");
}

prefs.begin("ticker", false);
prefs.remove("briIdx");  // legacy key, superseded by settings.h's "s.bri"
settingsLoad(prefs);

gfx.setRotation(gSettings.flip ? 3 : 1);
gfx.setBrightness(BRI_VAL[gSettings.briIdx]);
candleSeconds = settingsCandleSeconds();
jobs[0].interval = settingsPriceIntervalMs();

int loaded = storeInit(settingsCandleSeconds());
Serial.printf("store: %d candles loaded from flash\n", loaded);

// paint whatever's on flash immediately, before WiFi/NTP settle
renderIfDue(true);

WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
uint32_t loopStart = millis();
wifiSupervisor();
serviceJobs();
maybeBackfill();
candlesTick(lastPrice);
handleTouch();
maybeDailyRestart();
maybeHeapLog();
renderIfDue();
uint32_t workMs = millis() - loopStart;
delay(20);
uint32_t totalMs = millis() - loopStart;
if (totalMs > 0) {
uint8_t instPct = (uint8_t)min(100UL, (workMs * 100UL) / totalMs);
gCpuPct = (uint8_t)((gCpuPct * 3 + instPct) / 4);  // light EMA, avoids a jumpy readout
}
}
