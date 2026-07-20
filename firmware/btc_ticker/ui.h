#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Arduino.h>

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

// Installs the UI's colors as `spr`'s palette and remaps every COL_* from
// RGB565 to its palette index. Call once after createSprite() succeeds on a
// palette-depth sprite, before the first render. If never called (direct-to-
// gfx fallback), COL_* keep their RGB565 defaults.
void uiUsePalette(lgfx::LGFX_Sprite& spr);

// Renders the settings list page: grouped rows (MARKET DATA / CHART /
// DISPLAY) with section headers, value labels or toggle glyphs, shifted up
// by `scrollPx` (0 = top of the list; caller clamps to uiSettingsMaxScroll()).
void uiRenderSettings(lgfx::LovyanGFX* g, int scrollPx);

// Renders the option-picker page for settings row `row`: a one-line
// description of the setting, then every option as a radio row with the
// current one highlighted.
void uiRenderSettingsPicker(lgfx::LovyanGFX* g, int row);

// Renders the full-screen confirmation page shown before applying a
// destructive settings change (currently: candle size, which wipes the
// candle DB). `row`/`idx` are the pending change, shown as "OLD -> NEW".
void uiRenderConfirm(lgfx::LovyanGFX* g, int row, int idx);

// Renders the full-screen clock page. `mode` cycles on whole-screen tap:
//   0 = split (analog left 50% + 24h digital right 50%)
//   1 = big analog only
//   2 = big 24h digital only
// An "X" close control sits top-left (see UI_CLOCK_CLOSE_* hit zone).
void uiRenderClock(lgfx::LovyanGFX* g, uint8_t mode);

// Overlays the 3px touch-feedback border on all 4 sides of the current
// frame — drawn for one frame every time a touch registers. The right side
// stops at CONTENT_RIGHT.
void uiDrawTouchFlash(lgfx::LovyanGFX* g);

// Switches every UI color to red-only (night mode) or back to normal.
// Idempotent; call before each render with the currently-desired state.
void uiSetNightMode(bool on);

// Tells the UI where a finger is currently pressed (or that it was
// released), so renderers can draw pressed-state highlights (settings rows,
// gear button). Call on every touch state change, before renderIfDue(true).
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

// Dashboard: touch anywhere in this box (bottom-right corner) opens Settings.
// The gear button itself spans roughly x=288..316 (flush with the right edge
// every other element respects); this zone is deliberately wider for a
// comfortable touch target — nothing else in the footer is tappable.
static const int UI_GEAR_HIT_X0 = 230;
static const int UI_GEAR_HIT_Y0 = 200;

// Dashboard: tap the centered status-bar clock to enter full-screen clock.
// Generous for resistive touch — covers HH:MM:SS plus a little padding, and
// avoids the left status pulse/wifi and the right-side date.
static const int UI_CLOCK_HIT_X0 = 95;
static const int UI_CLOCK_HIT_X1 = 225;
static const int UI_CLOCK_HIT_Y0 = 0;
static const int UI_CLOCK_HIT_Y1 = 28;

// Full-screen clock page: "X" close control top-left. Hit zone is larger than
// the drawn glyph (resistive); drawing uses a 28x24 outlined icon button.
static const int UI_CLOCK_CLOSE_HIT_X0 = 0;
static const int UI_CLOCK_CLOSE_HIT_X1 = 44;
static const int UI_CLOCK_CLOSE_HIT_Y0 = 0;
static const int UI_CLOCK_CLOSE_HIT_Y1 = 36;
static const int UI_CLOCK_CLOSE_BTN_X  = 4;
static const int UI_CLOCK_CLOSE_BTN_Y  = 4;
static const int UI_CLOCK_CLOSE_BTN_W  = 28;
static const int UI_CLOCK_CLOSE_BTN_H  = 24;

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
