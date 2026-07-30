#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Arduino.h>
#include "net_weather.h"

// Device-status inputs shown in the shared footer (feed-status pulse, wifi
// glyph, CPU/RAM/ROM stats) — every full-screen page (dashboard, Clock,
// Weather, HOME) ends its render with uiDrawFooter(g, fs) using one of these,
// filled once per frame in btc_ticker.ino's renderIfDue().
struct UiFooterStatus {
  bool wifiConnected;
  uint32_t priceOkMs;  // millis() of last successful price fetch, 0 = never
  uint8_t cpuPct;      // 0-100, loop busy-time approximation
  uint8_t romPct;      // 0-100, sketch flash usage
  uint8_t ramPct;      // 0-100, heap usage
};

struct UiState {
  bool wifiConnected;
  uint32_t priceOkMs;  // millis() of last successful price fetch, 0 = never
  float price;         // NAN if none yet
  float changePct;     // NAN if unknown
  float dayHigh;       // 24h high from the ticker payload, NAN if unknown
  float dayLow;        // 24h low from the ticker payload, NAN if unknown
  uint8_t cpuPct;      // 0-100, loop busy-time approximation
  uint8_t romPct;      // 0-100, sketch flash usage
  uint8_t ramPct;      // 0-100, heap usage
};

// Renders the full 320x240 frame into `g` (the active draw target — palette
// sprite or direct panel, per the fallback in setup()) using the candle
// ring/forming candle from candles.h directly.
void uiRender(lgfx::LovyanGFX* g, const UiState& st);

// Draws the shared footer (feed-status pulse + wifi glyph, muted device
// stats, Home button) common to every full-screen page. Called at the end
// of uiRender() and of uiRenderClock()/uiRenderWeather()/uiRenderHome()/
// uiRenderSettings() (list page only — the picker/confirm sub-views don't
// draw it).
void uiDrawFooter(lgfx::LovyanGFX* g, const UiFooterStatus& fs);

// Footer Home button hit box, shared between drawing (uiDrawFooter) and
// hit-testing (btc_ticker.ino's handleTap) so they can't drift apart. Tapping
// here jumps straight to Page::HOME — the only way out of an app now that
// the long-press-to-home gesture is gone. Sized generously (most of the 28px
// footer band, well clear of the CPU/RAM/ROM stats text to its left) since
// resistive touch rewards a big, easy-to-land target over a tight one.
static const int UI_FOOTER_HOME_W  = 64;
static const int UI_FOOTER_HOME_H  = 26;
static const int UI_FOOTER_HOME_X1 = 316;  // CONTENT_RIGHT - EDGE
static const int UI_FOOTER_HOME_X0 = UI_FOOTER_HOME_X1 - UI_FOOTER_HOME_W;
static const int UI_FOOTER_HOME_Y0 = 213;  // just below the footer hairline (212)
static const int UI_FOOTER_HOME_Y1 = UI_FOOTER_HOME_Y0 + UI_FOOTER_HOME_H;

// Draws just the footer feed-status pulse dot (color + 250ms blink) into `g`.
// uiDrawFooter() calls this as part of a full frame; btc_ticker.ino's
// updateFeedPulse() also calls it directly against the live panel on its own
// fast cadence, bypassing the full-frame sprite render + SPI push so the
// blink stays smooth without redrawing the whole dashboard 4x as often.
void uiDrawFeedPulse(lgfx::LovyanGFX* g, bool wifiConnected, uint32_t priceOkMs);

// Installs the UI's colors as `spr`'s palette and remaps every COL_* from
// RGB565 to its palette index. Call once after createSprite() succeeds on a
// palette-depth sprite, before the first render. If never called (direct-to-
// gfx fallback), COL_* keep their RGB565 defaults.
void uiUsePalette(lgfx::LGFX_Sprite& spr);

// Renders the settings list page: grouped rows (GLOBAL / BTC TICKER) with
// section headers, value labels or toggle glyphs, shifted up by `scrollPx`
// (0 = top of the list; caller clamps to uiSettingsMaxScroll()). Ends with
// the shared footer (`fs`) below the list — the picker/confirm sub-views
// (uiRenderSettingsPicker/uiRenderConfirm) don't draw it.
void uiRenderSettings(lgfx::LovyanGFX* g, int scrollPx, const UiFooterStatus& fs);

// Renders the option-picker page for settings row `row`: a one-line
// description of the setting, then every option as a radio row with the
// current one highlighted.
void uiRenderSettingsPicker(lgfx::LovyanGFX* g, int row);

// Renders the full-screen confirmation page shown before applying a
// destructive settings change (currently: candle size, which wipes the
// candle DB). `row`/`idx` are the pending change, shown as "OLD -> NEW".
void uiRenderConfirm(lgfx::LovyanGFX* g, int row, int idx);

// Renders the full-screen "Find Access Mode" Wi-Fi setup page shown after
// confirming Forget Wi-Fi network: `apSsid` (the SoftAP network name) +
// portal URL + a Cancel button (reboots immediately, which is safe — see
// wifi_creds.h/wifi_portal.h for why an aborted setup always falls back to
// the compiled config.h network). Takes the SSID as a parameter rather than
// including wifi_portal.h, so ui.cpp stays presentation-only.
void uiRenderWifiSetup(lgfx::LovyanGFX* g, const char* apSsid);

// Renders the full-screen boot splash shown only when there is no cached
// candle data yet (fresh device, or right after a candle-size change wipes
// the DB) — any reset with existing flash data paints the dashboard from
// that cache immediately instead (see setup()'s "paint whatever's on flash"
// comment). No touch controls; the .ino exits back to the dashboard on the
// first successful price fetch, or after a timeout if Wi-Fi/the feed never
// comes up. `wifiConnected` picks the status line; `elapsedMs` (millis()
// since the splash started) drives the 3-dot loader phase.
void uiRenderSplash(lgfx::LovyanGFX* g, bool wifiConnected, uint32_t elapsedMs);

// Renders the full-screen clock page. `mode` cycles on whole-screen tap:
//   0 = split (analog left 50% + 24h digital right 50%)
//   1 = big analog only
//   2 = big 24h digital only
// No in-page close control — exit is the shared footer's Home button
// (btc_ticker.ino's handleTap()). Ends with the shared footer (`fs`) below
// the clock face(s).
void uiRenderClock(lgfx::LovyanGFX* g, uint8_t mode, const UiFooterStatus& fs);

// Renders the full-screen Weather page: current conditions + today's hi/lo
// (top), the next WEATHER_NUM_HOURS hours (middle strip), and
// WEATHER_NUM_DAYS days of forecast as rows with an iOS-style range bar
// scaled to the week's overall min/max (bottom). `okMs` is millis() of the
// last successful fetch (0 = never) for the "updated Nm ago" caption. All
// hour/weekday labels use wx.tzOffset (the weather location's own UTC
// offset), not the device's configured TZ. `city` is the caption in the
// top-left corner, taken as a parameter (like uiRenderWifiSetup()'s SSID)
// rather than #including config.h here, so ui.cpp stays presentation-only.
// No in-page controls — exit is the shared footer's Home button. Ends with
// the shared footer (`fs`) below the 5-day forecast.
void uiRenderWeather(lgfx::LovyanGFX* g, const WeatherData& wx, uint32_t okMs, const char* city,
                     const UiFooterStatus& fs);

// Renders the full-screen Calendar page: a single month view (no other
// views/navigation) for the current month/year off the device's synced
// clock, Sunday-first, with today's cell highlighted. No in-page controls —
// exit is the shared footer's Home button. Ends with the shared footer
// (`fs`) below the grid.
void uiRenderCalendar(lgfx::LovyanGFX* g, const UiFooterStatus& fs);

// Renders the HOME launcher: a 4-column x 3-row grid of square app tiles
// (iPad-style), filled left-to-right/top-to-bottom starting with BTC
// TICKER, CLOCK, Settings — remaining slots stay empty so future apps can
// drop into the next free slot with no layout change. This is the default
// landing page; apps are entered by tapping a tile and left only via the
// footer Home button (no in-app back/close controls). Ends with the shared
// footer (`fs`) — the empty bottom grid rows leave room for it.
void uiRenderHome(lgfx::LovyanGFX* g, const UiFooterStatus& fs);

// Overlays the 3px touch-feedback border on all 4 sides of the current
// frame — drawn for one frame every time a touch registers. The right side
// stops at CONTENT_RIGHT.
void uiDrawTouchFlash(lgfx::LovyanGFX* g);

// Switches every UI color to red-only (night mode) or back to normal.
// Idempotent; call before each render with the currently-desired state.
void uiSetNightMode(bool on);

// Tells the UI where a finger is currently pressed (or that it was
// released), so renderers can draw pressed-state highlights (settings rows,
// home tiles). Call on every touch state change, before renderIfDue(true).
void uiSetPressedPoint(int x, int y, bool down);

// Maps a tap at absolute screen y `ty` (below the title bar) to a settings
// row enum value, given the current scroll offset. Returns -1 for section
// headers and empty space. Shared between drawing and hit-testing so the
// grouped list layout can't drift apart.
int uiSettingsItemAt(int scrollPx, int ty);

// Total scrollable px of the grouped settings list beyond the viewport
// (>= 0). Caller clamps settingsScroll to this.
int uiSettingsMaxScroll();

// Maps a tap at absolute screen y `ty` on the picker page to an option
// index for `row`, or -1 if the tap missed every option row. Shares the
// picker's layout math with uiRenderSettingsPicker.
int uiPickerOptionAt(int row, int ty);

// Maps a tap at absolute screen (x, y) on the HOME page to a tile slot
// index (0..UI_HOME_SLOT_COUNT-1, row-major left-to-right/top-to-bottom),
// or -1 if the tap missed every tile / hit an empty slot. Shares the grid
// layout math with uiRenderHome so drawing and hit-testing can't drift.
int uiHomeTileAt(int x, int y);

// HOME grid geometry: 4 columns x 3 rows of square tiles, only the first
// UI_HOME_APP_COUNT slots populated (BTC TICKER, CLOCK, Settings, Weather,
// Calendar — in that slot order, matched to btc_ticker.ino's HOME_SLOT_PAGE
// and this file's HOME_APP_LABELS/uiRenderHome icon dispatch). Shared
// between uiRenderHome and uiHomeTileAt.
static const int UI_HOME_COLS = 4;
static const int UI_HOME_ROWS = 3;
static const int UI_HOME_SLOT_COUNT = UI_HOME_COLS * UI_HOME_ROWS;
static const int UI_HOME_APP_COUNT = 5;

// Clock display modes for uiRenderClock / cycle-on-tap.
static const uint8_t UI_CLOCK_MODE_SPLIT  = 0;
static const uint8_t UI_CLOCK_MODE_ANALOG = 1;
static const uint8_t UI_CLOCK_MODE_DIGITAL = 2;
static const uint8_t UI_CLOCK_MODE_COUNT  = 3;

// Settings page chrome, shared between drawing (ui.cpp) and hit-testing
// (btc_ticker.ino) so they can't drift apart. List layout itself (row
// heights, scroll range, item hit-testing) lives behind the helpers above —
// don't duplicate it.
static const int UI_SET_TITLE_H = 30;
static const int UI_BACK_HIT_X1 = 90;  // tap left of this, in the title bar, goes back

// Confirmation page buttons (uiRenderConfirm), shared with hit-testing.
static const int UI_CONFIRM_Y0 = 168;
static const int UI_CONFIRM_Y1 = 202;
static const int UI_CONFIRM_CANCEL_X0 = 30;
static const int UI_CONFIRM_CANCEL_X1 = 150;
static const int UI_CONFIRM_OK_X0 = 170;
static const int UI_CONFIRM_OK_X1 = 290;

// Wi-Fi setup page (uiRenderWifiSetup) Cancel button, shared with hit-testing.
static const int UI_WIFI_SETUP_CANCEL_X0 = 100;
static const int UI_WIFI_SETUP_CANCEL_X1 = 220;
static const int UI_WIFI_SETUP_CANCEL_Y0 = 190;
static const int UI_WIFI_SETUP_CANCEL_Y1 = 224;
