// BTC/USDT ticker + clock + candlestick dashboard for the CYD board
// (ESP32-2432S028R). Price from Binance REST, up to 288 candles with a
// LittleFS ring-file mini-database surviving reboots/power loss. Candle
// size, chart range, price fetch cadence, chart style, brightness, night
// mode (red-only UI 23:00-08:00, plus a manual force-on), and screen
// orientation are all user-configurable from the Settings page (gear
// button, bottom right).

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
#include "wifi_creds.h"
#include "wifi_portal.h"

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
float dayHigh = NAN;  // 24h high/low from the ticker payload, for the range bar
float dayLow = NAN;
uint32_t priceOkMs = 0;

enum class Page : uint8_t { DASHBOARD, SETTINGS, CLOCK, WIFI_SETUP, SPLASH };
static Page currentPage = Page::DASHBOARD;
// Set when setup() enters Page::SPLASH (no cached candles to paint yet);
// millis() timestamp the loader phase and the exit timeout are measured from.
static uint32_t splashStartMs = 0;
// Bounded so a Wi-Fi/feed outage can't leave the splash up forever — the
// dashboard's own CONNECTING/OFFLINE placeholders (ui.cpp) take over instead.
static const uint32_t SPLASH_TIMEOUT_MS = 20000;
static int settingsScroll = 0;  // px, 0..uiSettingsMaxScroll()
static int pickerRow = -1;      // -1 = settings list, >=0 = option picker for that row
static int confirmIdx = -1;     // >=0 = confirmation page showing, pending option idx for pickerRow
static uint8_t clockMode = UI_CLOCK_MODE_SPLIT;  // cycles on whole-screen tap

static void renderIfDue(bool force = false);
static void applySettingChange(uint16_t mask);

// ── touch ──────────────────────────────────────────────────
// Dashboard: gear opens Settings; status-bar clock opens full-screen clock.
// Full-screen clock: "X" top-left returns to dashboard; any other tap cycles
// split → analog → digital → split. Settings list: drag scrolls; tap on a
// binary row (On/Off) toggles it in place, tap on a multi-choice row opens
// its option picker. Picker: tap an option to select it and return —
// except candle size, which first shows a confirmation page because it
// wipes the candle DB. Taps fire on release so a drag that starts on a row
// doesn't also select it.
// Feedback: while any touch registers, renderIfDue overlays a 3px 4-side
// border (uiDrawTouchFlash) for one frame per detection, and the pressed
// row/button highlights (uiSetPressedPoint).
static int pressX = 0, pressY = 0;
static bool dragging = false;
static int dragStartScroll = 0;
static const int DRAG_THRESHOLD_PX = 10;  // resistive touch jitters a few px
static bool touchFlashOn = false;  // touch level at the last handleTouch pass

// Persists which of the two restorable pages (dashboard or full-screen
// clock, plus clock face) is current, so setup() can bring it back after a
// reset. Settings/Wi-Fi-setup aren't restorable — call sites only invoke
// this on transitions into/within DASHBOARD or CLOCK.
static void saveLastPage() {
if (currentPage != Page::DASHBOARD && currentPage != Page::CLOCK) return;
prefs.putUChar("s.page", currentPage == Page::CLOCK ? 1 : 0);
prefs.putUChar("s.clkMode", clockMode);
}

// Persist every row named by a settingsSet() change-mask and apply side
// effects (brightness, job intervals, candle reset, ...).
static void applyMask(uint16_t mask) {
for (int r = 0; r < ROW_COUNT; r++) {
if (mask & (1u << r)) settingsSaveRow(prefs, r);
}
applySettingChange(mask);
}

static void handleTap(int tx, int ty) {
if (currentPage == Page::SPLASH) return;  // no controls during boot

if (currentPage == Page::WIFI_SETUP) {
// Cancel is the only control on this page; reboots immediately. Safe
// no-op — credentials were already cleared and nothing new was saved,
// so the next boot falls back to the compiled config.h network.
if (tx >= UI_WIFI_SETUP_CANCEL_X0 && tx < UI_WIFI_SETUP_CANCEL_X1 &&
    ty >= UI_WIFI_SETUP_CANCEL_Y0 && ty < UI_WIFI_SETUP_CANCEL_Y1) {
esp_restart();
}
return;
}

if (currentPage == Page::CLOCK) {
// Close "X" top-left — generous hit zone shared with ui.h drawing.
if (tx >= UI_CLOCK_CLOSE_HIT_X0 && tx < UI_CLOCK_CLOSE_HIT_X1 &&
    ty >= UI_CLOCK_CLOSE_HIT_Y0 && ty < UI_CLOCK_CLOSE_HIT_Y1) {
currentPage = Page::DASHBOARD;
saveLastPage();
renderIfDue(true);
return;
}
// Whole-screen tap cycles the three clock presentations.
clockMode = (uint8_t)((clockMode + 1) % UI_CLOCK_MODE_COUNT);
saveLastPage();
renderIfDue(true);
return;
}

if (currentPage == Page::DASHBOARD) {
if (tx >= UI_GEAR_HIT_X0 && ty >= UI_GEAR_HIT_Y0) {
currentPage = Page::SETTINGS;
settingsScroll = 0;
pickerRow = -1;
confirmIdx = -1;
renderIfDue(true);
} else if (gSettings.showClock &&
           tx >= UI_CLOCK_HIT_X0 && tx < UI_CLOCK_HIT_X1 &&
           ty >= UI_CLOCK_HIT_Y0 && ty < UI_CLOCK_HIT_Y1) {
currentPage = Page::CLOCK;
// Keep last clockMode so re-entry restores the user's preferred face.
saveLastPage();
renderIfDue(true);
}
return;
}

// Confirmation page (a destructive change is pending) — modal: only the
// two buttons respond.
if (confirmIdx >= 0 && pickerRow >= 0) {
if (ty >= UI_CONFIRM_Y0 && ty < UI_CONFIRM_Y1) {
if (tx >= UI_CONFIRM_OK_X0 && tx < UI_CONFIRM_OK_X1) {
if (pickerRow == ROW_FORGET_AP) {
// Not a settingsSet() value change — tear down STA and hand the
// device to the phone-facing setup portal. fetchPriceRelease()
// frees the price poll's TLS session first (one session at a time,
// same discipline as maybeBackfill()).
fetchPriceRelease();
wifiCredsClear();
wifiPortalStart();
currentPage = Page::WIFI_SETUP;
} else {
applyMask(settingsSet(pickerRow, (uint8_t)confirmIdx));
}
confirmIdx = -1;
pickerRow = -1;  // back to the list, which now shows the new value
renderIfDue(true);
} else if (tx >= UI_CONFIRM_CANCEL_X0 && tx < UI_CONFIRM_CANCEL_X1) {
if (pickerRow == ROW_FORGET_AP) pickerRow = -1;  // no options page to return to
confirmIdx = -1;  // back to the picker (or list), nothing changed
renderIfDue(true);
}
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
int idx = uiPickerOptionAt(pickerRow, ty);
if (idx >= 0) {
if (idx == settingsOptionIndex(pickerRow)) {
pickerRow = -1;  // re-tapped the current value: just close the picker
renderIfDue(true);
} else if (pickerRow == ROW_CANDLE_IV) {
// Destructive (wipes ring + flash DB) — confirm before applying.
confirmIdx = idx;
renderIfDue(true);
} else {
applyMask(settingsSet(pickerRow, (uint8_t)idx));
pickerRow = -1;  // back to the list, which now shows the new value
renderIfDue(true);
}
}
return;
}

// Settings list page
if (ty < UI_SET_TITLE_H) {
if (tx < UI_BACK_HIT_X1) {
currentPage = Page::DASHBOARD;
saveLastPage();
renderIfDue(true);
}
return;
}
int row = uiSettingsItemAt(settingsScroll, ty);
if (row >= 0) {
if (row == ROW_FORGET_AP) {
// Action row, not a value — skip the option picker and go straight
// to the confirmation page (confirmIdx is unused for this row beyond
// being >= 0; see handleTap's confirm-page branch above).
confirmIdx = 0;
pickerRow = row;
} else if (settingsOptionCount(row) == 2) {
// Binary setting: the row IS the switch — flip in place, no picker.
applyMask(settingsSet(row, (uint8_t)(1 - settingsOptionIndex(row))));
} else {
pickerRow = row;
}
renderIfDue(true);
}
}

void handleTouch() {
int32_t tx, ty;
bool touchDown = gfx.getTouch(&tx, &ty);
uint32_t now = millis();

touchFlashOn = touchDown;
// Keep the UI's pressed-point state current BEFORE the edge render below,
// so pressed-state highlights appear/clear on the same frame.
if (touchDown) uiSetPressedPoint(tx, ty, true);
else uiSetPressedPoint(pressX, pressY, false);
// Force a render on both touch edges so the feedback border appears the
// moment a touch registers and clears the moment it releases, instead of
// waiting for the next 1Hz tick.
if (touchDown != touchWasDown) renderIfDue(true);

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
int maxScroll = uiSettingsMaxScroll();
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
float p, c, h, l;
if (!fetchPrice(p, c, h, l)) return false;
lastPrice = p;
if (!isnan(c)) changePct = c;
if (!isnan(h)) dayHigh = h;
if (!isnan(l)) dayLow = l;
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

// Doubling backoff, capped at RETRY_MAX_MS — shared by the job scheduler
// below and maybeBackfill()'s own retry timer.
static uint32_t nextBackoff(uint32_t cur) {
return cur * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : cur * 2;
}

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
j.backoff = nextBackoff(j.backoff);
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
backfillBackoff = nextBackoff(backfillBackoff);
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

// ── boot splash exit (checked every loop() pass while it's showing) ──
// Leaves Page::SPLASH the moment there's real data to show (first successful
// price fetch), or unconditionally after SPLASH_TIMEOUT_MS so a Wi-Fi/feed
// outage can't strand the user on the loading screen — either way the
// dashboard itself already knows how to render "still connecting"/"offline".
static void maybeExitSplash() {
if (currentPage != Page::SPLASH) return;
if (priceOkMs != 0 || millis() - splashStartMs > SPLASH_TIMEOUT_MS) {
currentPage = Page::DASHBOARD;
renderIfDue(true);
}
}

// ── apply a settings change live (called right after settingsCycle) ──
static void applySettingChange(uint16_t mask) {
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
// ROW_STYLE, ROW_NIGHT, ROW_NIGHT_FORCE, ROW_RANGEBAR, ROW_SHOW_PRICE,
// ROW_SHOW_DATE and ROW_SHOW_CLOCK need no side effect — the next render
// reads gSettings / applies uiSetNightMode directly (and reflows chart
// height when price / range bar / status bar are hidden).
}

// ── WiFi supervisor ────────────────────────────────────────
static uint32_t wifiDownSinceMs = 0;
static uint32_t wifiLastRetryMs = 0;

// Connects using stored NVS credentials if present (set via the "Forget
// Wi-Fi network" setup portal), else the compiled config.h defaults. Shared
// between setup() and wifiSupervisor()'s re-begin() so the fallback logic
// can't drift between the two call sites.
static void wifiBeginConfigured() {
char ssid[WIFI_CREDS_SSID_LEN];
char pass[WIFI_CREDS_PASS_LEN];
if (wifiCredsLoad(ssid, sizeof(ssid), pass, sizeof(pass))) {
WiFi.begin(ssid, pass);
} else {
WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}
}

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
wifiBeginConfigured();
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

static void maybeHeapLog(uint8_t cpuPct) {
  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 60000UL) return;
  last = now;
  Serial.printf("heap: %u free  cpu: %u%%\n", (unsigned)ESP.getFreeHeap(), (unsigned)cpuPct);
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

// ── render (1Hz, or forced on touch edges/taps/scrolls) ────
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

// Auto-brightness from the on-board LDR (CYD_LDR / GPIO34).
//
// The naive version (single analogRead + hard thresholds every render) flashed
// the screen: ESP32 ADC noise of a few hundred counts around a threshold made
// the duty hop between adjacent BRI_VAL steps at 1Hz (and more often when
// touch force-renders). Fix:
//   1) multi-sample average to kill high-frequency ADC noise
//   2) hysteresis so a level only changes after the LDR has clearly crossed
//   3) applyBrightnessDuty only calls setBrightness when the duty changes
//      (PWM reconfig on every frame can also glitch the backlight)
//
// Nominal thresholds (no hyst): <800→5%, <1500→25%, <2200→50%, <3000→75%, else 100%.
static const int AUTO_BRI_THRESH[] = {800, 1500, 2200, 3000};
static const int AUTO_BRI_HYST = 200;       // ADC counts of deadband around each edge
static const int AUTO_BRI_SAMPLES = 8;      // average this many analogRead()s
static const uint32_t AUTO_BRI_MIN_MS = 1500; // don't re-decide more often than this

static uint8_t gAutoBriLevel = 3;          // last selected level 1..5 (start mid: 3 = 50%)
static uint32_t gAutoBriLastMs = 0;
static uint8_t gLastBriDuty = 0xFF;        // last duty pushed to the panel

static int readLdrAveraged() {
  long sum = 0;
  for (int i = 0; i < AUTO_BRI_SAMPLES; i++) {
    sum += analogRead(CYD_LDR);
  }
  return (int)(sum / AUTO_BRI_SAMPLES);
}

// Map averaged LDR → BRI_VAL index with hysteresis relative to gAutoBriLevel.
// Rate-limited so touch-forced renders don't re-sample every few ms.
static uint8_t getAutoBrightnessVal() {
  uint32_t now = millis();
  if (gAutoBriLastMs != 0 && (now - gAutoBriLastMs) < AUTO_BRI_MIN_MS) {
    return BRI_VAL[gAutoBriLevel];
  }
  gAutoBriLastMs = now;

  int ldr = readLdrAveraged();
  uint8_t level = gAutoBriLevel;

  // Climb while clearly above the upward threshold for the next step.
  while (level < 5 && ldr >= AUTO_BRI_THRESH[level - 1] + AUTO_BRI_HYST) {
    level++;
  }
  // Drop while clearly below the downward threshold for the current step.
  while (level > 1 && ldr < AUTO_BRI_THRESH[level - 2] - AUTO_BRI_HYST) {
    level--;
  }

  if (level != gAutoBriLevel) {
    Serial.printf("auto-bri: ldr=%d level %u→%u duty=%u\n",
                  ldr, (unsigned)gAutoBriLevel, (unsigned)level,
                  (unsigned)BRI_VAL[level]);
    gAutoBriLevel = level;
  }
  return BRI_VAL[gAutoBriLevel];
}

// Push a PWM duty to the backlight only when it actually changes. Avoids a
// redundant Light_PWM reconfig every render (and the brief flash that can
// cause on some CYD backlight drivers).
static void applyBrightnessDuty(uint8_t duty) {
  if (duty == gLastBriDuty) return;
  gfx.setBrightness(duty);
  gLastBriDuty = duty;
}

// Dims to 1% brightness while night mode is active, restoring the user's
// chosen brightness setting the rest of the time. Duty is memoised inside
// applyBrightnessDuty so a brightness-setting change made *while* night
// mode is active still takes effect the moment night ends (or the moment
// the user picks a new fixed level / Auto).
static void applyNightBrightness(bool on) {
  if (on) {
    applyBrightnessDuty(BRI_VAL[0]);  // 1% — see settings.cpp
  } else if (gSettings.briIdx == 6) {
    applyBrightnessDuty(getAutoBrightnessVal());
  } else {
    applyBrightnessDuty(BRI_VAL[gSettings.briIdx]);
  }
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
  if (confirmIdx >= 0 && pickerRow >= 0) uiRenderConfirm(g, pickerRow, confirmIdx);
  else if (pickerRow >= 0) uiRenderSettingsPicker(g, pickerRow);
  else uiRenderSettings(g, settingsScroll);
} else if (currentPage == Page::CLOCK) {
  uiRenderClock(g, clockMode);
} else if (currentPage == Page::WIFI_SETUP) {
  uiRenderWifiSetup(g, WIFI_PORTAL_SSID);
} else if (currentPage == Page::SPLASH) {
  uiRenderSplash(g, WiFi.status() == WL_CONNECTED, millis() - splashStartMs);
} else {
UiState st;
st.wifiConnected = WiFi.status() == WL_CONNECTED;
st.priceOkMs = priceOkMs;
st.price = lastPrice;
st.changePct = changePct;
st.dayHigh = dayHigh;
st.dayLow = dayLow;
st.cpuPct = gCpuPct;
st.romPct = romUsagePct();
st.ramPct = ramUsagePct();
uiRender(g, st);
}
if (touchFlashOn) uiDrawTouchFlash(g);  // one-frame border per touch register
presentFrame();
}

// Redraws just the footer feed-status dot, direct-to-panel (bypassing the
// sprite + full uiRender()/presentFrame() path), so the 250ms blink (ui.cpp)
// is visible without paying for a full-frame redraw 4x as often — that
// approach previously pegged CPU% near 100% (renderIfDue()'s dashboard
// interval briefly dropped to 250ms to chase this same blink; reverted).
// Only valid while the dashboard is actually the page on screen.
static void updateFeedPulse() {
static uint32_t lastMs = 0;
if (currentPage != Page::DASHBOARD) return;
uint32_t now = millis();
if (now - lastMs < 125) return;  // comfortably under the 250ms toggle period
lastMs = now;
uiDrawFeedPulse(&gfx, WiFi.status() == WL_CONNECTED, priceOkMs);
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

// Restore the page the user was actually looking at (dashboard or one of the
// full-screen clock faces) so a reset/power-cycle doesn't bounce back to the
// dashboard. Settings/Wi-Fi-setup are transient and never saved here (see
// saveLastPage()), so a reset mid-settings falls back to the dashboard.
clockMode = prefs.getUChar("s.clkMode", UI_CLOCK_MODE_SPLIT);
if (clockMode >= UI_CLOCK_MODE_COUNT) clockMode = UI_CLOCK_MODE_SPLIT;
currentPage = (prefs.getUChar("s.page", 0) == 1) ? Page::CLOCK : Page::DASHBOARD;

gfx.setRotation(gSettings.flip ? 3 : 1);
// Seed backlight before the first paint. gLastBriDuty starts as 0xFF so the
// first applyBrightnessDuty always writes; subsequent renderIfDue calls
// only rewrite when the duty actually changes.
if (gSettings.briIdx == 6) {
  applyBrightnessDuty(getAutoBrightnessVal());
} else {
  applyBrightnessDuty(BRI_VAL[gSettings.briIdx]);
}
candleSeconds = settingsCandleSeconds();
jobs[0].interval = settingsPriceIntervalMs();

int loaded = storeInit(settingsCandleSeconds());
Serial.printf("store: %d candles loaded from flash\n", loaded);

// Fresh device (or a DB just wiped by a candle-size change): nothing useful
// to paint from flash, so show the boot splash instead of the dashboard's
// bare "--" price / "OFFLINE — NO CACHED DATA" placeholder while Wi-Fi
// connects and the first price payload arrives. Skipped if the restored
// page is the full-screen clock — that doesn't need price data at all.
if (loaded == 0 && currentPage == Page::DASHBOARD) {
  currentPage = Page::SPLASH;
  splashStartMs = millis();
}

// paint whatever's on flash immediately, before WiFi/NTP settle
renderIfDue(true);

// Creds are managed in our own "wifi" NVS namespace (wifi_creds.cpp); disable the
// WiFi driver's own persistence so its repeated begin() calls in wifiSupervisor()
// don't churn the shared NVS partition and risk a boot-time full-partition erase.
WiFi.persistent(false);
WiFi.mode(WIFI_STA);
WiFi.setAutoReconnect(true);
wifiBeginConfigured();
}

void loop() {
uint32_t loopStart = millis();
if (currentPage == Page::WIFI_SETUP) {
// No internet in AP mode — none of the normal 24/7 job/backfill/restart
// logic has anything to do. Just service the setup portal and touch.
wifiPortalLoop();
handleTouch();
renderIfDue();
delay(10);
return;
}
wifiSupervisor();
serviceJobs();
maybeBackfill();
candlesTick(lastPrice);
maybeExitSplash();
handleTouch();
maybeDailyRestart();
maybeHeapLog(gCpuPct);
renderIfDue();
updateFeedPulse();
uint32_t workMs = millis() - loopStart;
delay(20);
uint32_t totalMs = millis() - loopStart;
if (totalMs > 0) {
uint8_t instPct = (uint8_t)min(100UL, (workMs * 100UL) / totalMs);
gCpuPct = (uint8_t)((gCpuPct * 3 + instPct) / 4);  // light EMA, avoids a jumpy readout
}
}
