#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <Arduino.h>

struct UiState {
  bool wifiConnected;
  uint32_t priceOkMs;  // millis() of last successful price fetch, 0 = never
  float price;         // NAN if none yet
  float changePct;     // NAN if unknown
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

// Renders the settings list page.
void uiRenderSettings(lgfx::LovyanGFX* g);

// Switches every UI color to red-only (night mode) or back to normal.
// Idempotent; call before each render with the currently-desired state.
void uiSetNightMode(bool on);

// Dashboard: touch anywhere in this box (top-right corner) opens Settings.
// Gear glyph itself spans roughly x=282..300 (flush with the 20px right-edge
// padding every other element respects); this zone is deliberately wider for
// a comfortable touch target — nothing else in the header is tappable.
static const int UI_GEAR_HIT_X0 = 270;
static const int UI_GEAR_HIT_Y1 = 44;

// Settings page layout, shared between drawing (ui.cpp) and hit-testing
// (btc_ticker.ino) so they can't drift apart. 30 + 8 rows x 26 = 238 of 240.
static const int UI_SET_TITLE_H = 30;
static const int UI_SET_ROW_H = 26;
static const int UI_BACK_HIT_X1 = 90;  // tap left of this, in the title bar, goes back
