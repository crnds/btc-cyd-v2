# AGENTS.md

Guidance for AI coding agents working on this repository. Assumes no prior
knowledge of the project.

## Project overview

**btc-cyd-v2** is firmware for a 24/7 BTC/USDT ticker, clock, and candlestick
dashboard running on the CYD ("Cheap Yellow Display") board
**ESP32-2432S028R**: ESP32-D0WD-V3, 4MB flash, 320x240 ILI9341 TFT, XPT2046
resistive touch, connected via USB serial at `/dev/cu.usbserial-110`.

What it does:

- Polls BTC/USDT price + 24h change from the Binance public REST API
  (`/api/v3/ticker/24hr`, keyless) at a configurable cadence (default 1s).
- Assembles up to 288 candles (default 5m = 24h) in a RAM ring and persists
  them in a LittleFS flash "mini-database" (`/candles.bin`) that survives
  reboots and power loss; backfills from `/api/v3/klines` on boot and repairs
  gaps after reconnects.
- Shows clock (NTP, timezone `ICT-7` / Asia/Bangkok), big price, candlestick
  chart, and device stats; tap the status-bar clock for a full-screen ambient
  clock (split analog+24h digital, or analog/digital only — cycle by tap;
  "X" top-left returns); a touch-driven Settings page (gear icon,
  bottom-right) configures brightness, 180° flip, price/candle intervals,
  time range, chart style (Red/Green, Black/White, Line), and night mode
  (red-only UI + 5% brightness 23:00-08:00, plus manual force-on); a
  "Forget Wi-Fi network" row under a NETWORK section clears any stored
  Wi-Fi credentials and puts the device into a SoftAP + captive-portal
  setup mode ("Find Access Mode") so a phone can submit a new network
  without a reflash.
- 24/7 robustness: job scheduler with exponential backoff, WiFi supervisor
  (re-`begin()` after 60s down, `esp_restart()` after 10min), daily 4AM
  restart, heap logging every 60s.

The repo also contains **`simulator.html`**, a self-contained browser-based
simulator of the device: it mirrors the firmware's logic and constants in
JavaScript, emulates the LovyanGFX display on a canvas, fetches live Binance
data, persists settings/candles in `localStorage`, and offers diagnostics
(WiFi/API toggles, LittleFS sector corruption, DB wipe, serial log console).
Open it directly in any modern browser — no build step.

## Tech stack

- **Language/framework**: Arduino framework on ESP32 (C++), plain
  `.ino`/`.h`/`.cpp` sketch — **arduino-cli**, not PlatformIO.
- **Required toolchain/libs** (already expected installed):
  `arduino-cli`, esp32 core **3.3.10**, LovyanGFX **1.2.25**, ArduinoJson
  **7.4.3**. WiFi, HTTPClient, WiFiClientSecure (mbedTLS), LittleFS, and
  Preferences come with the esp32 core.
- **FQBN**: `esp32:esp32:esp32:PartitionScheme=huge_app,UploadSpeed=115200`.
  The `huge_app` partition scheme (3MB app / 1MB FS) is mandatory — WiFi +
  TLS + LovyanGFX exceed the default 1.2MB app slot.
- **Simulator**: single-file vanilla HTML/CSS/JS, no dependencies except
  Google Fonts and the live Binance API.

## Build, flash, run

```sh
# one-time: create WiFi config (gitignored)
cp firmware/btc_ticker/config.example.h firmware/btc_ticker/config.h
# edit config.h with your WiFi SSID/password

make build     # arduino-cli compile
make flash     # compile + upload to /dev/cu.usbserial-110
make monitor   # serial monitor @ 115200
make clean     # rm -rf firmware/btc_ticker/build
```

All Make targets wrap `arduino-cli`; the FQBN, port, and baud live at the top
of the `Makefile`.

## Repository layout

```
Makefile                   # build / flash / monitor / clean (arduino-cli)
README.md                  # quickstart
plan.md                    # original design doc (design rationale, verification plan)
future-plan.md             # idea catalog for future features (not approved work)
CLAUDE.md                  # UI conventions (also covered below)
simulator.html             # browser hardware simulator (single file)
firmware/btc_ticker/       # the arduino-cli sketch (dir name == sketch name)
├── btc_ticker.ino         # LGFX display/touch class, sprite, setup()/loop(),
│                          #   job scheduler (round-robin + exp backoff), backfill/
│                          #   gap-repair, WiFi supervisor, daily restart, heap log,
│                          #   touch (tap/drag), page state (dashboard /
│                          #   settings / picker / confirm), night-mode logic
├── pins.h                 # CYD pin mapping (see hardware notes below)
├── config.example.h       # template; copy to config.h (gitignored, WiFi creds)
├── candles.h/.cpp         # CandleRec, candleRing[288], forming candle,
│                          #   assembly/close, gap detection
├── store.h/.cpp           # LittleFS ring-file persistence (/candles.bin)
├── net_http.h/.cpp        # one-shot HTTP/1.0 GET helper (unchunked)
├── net_price.h/.cpp       # persistent keep-alive TLS session for price polls
├── net_klines.h/.cpp      # streaming klines parser (no payload buffering)
├── settings.h/.cpp        # settings model, NVS persistence, option pickers
├── wifi_creds.h/.cpp      # NVS-backed Wi-Fi credentials (separate "wifi"
│                          #   namespace), fallback to config.h when empty
├── wifi_portal.h/.cpp     # SoftAP + captive portal for phone-driven Wi-Fi
│                          #   re-provisioning ("Forget Wi-Fi network")
└── ui.h/.cpp              # all drawing: dashboard, grouped settings list,
                           #   option pickers, confirm dialog, Wi-Fi setup
                           #   page, night mode
```

## Module notes and key designs

- **Candle engine** (`candles.*`): `CandleRec` is a 20-byte packed struct
  `{openEpoch, o, h, l, c}`. The ring slot is a pure function of time:
  `slot = (openEpoch / candleSeconds) % MAX_CANDLES` (`MAX_CANDLES = 288`),
  so out-of-order backfill writes are harmless. `candlesTick(price)` runs
  every loop pass and closes candles on bucket boundaries even while WiFi is
  down. Candle width (`candleSeconds`) is runtime-configurable; changing it
  must go through `candlesReset()` (slot math changes, ring + flash wiped).
- **Flash store** (`store.*`): `/candles.bin` = 8-byte header
  `{magic 'CND1', intervalSec}` + 288 fixed-size records. On load every
  record is validated (epoch alignment, recency, OHLC consistency, sane
  price range); invalid records become gaps, which backfill then repairs.
  LittleFS is mounted with `formatOnFail`.
- **Networking** (`net_*`): two deliberate strategies. Price polling keeps
  ONE persistent HTTP/1.1 keep-alive TLS session (a fresh handshake per 1Hz
  poll is too expensive); never call `useHTTP10()` there — it kills
  keep-alive. One-shot fetches (klines) use `net_http`'s HTTP/1.0 helper so
  the body is unchunked and can be stream-parsed. The klines parser is a
  bracket-depth scanner that extracts fields off the TLS stream with zero
  buffering (the payload is ~90KB at limit=288).
- **Heap discipline** (tight heap, important): only one TLS session at a
  time — each mbedTLS session holds ~45KB, so `fetchPriceRelease()` is
  called before a klines backfill (and after price polls when the interval
  is >= 60s). The backfill staging buffer is `malloc`'d (heap-transient),
  not stack — the loop task has only an 8KB stack.
- **Display**: full-frame 320x240 sprite in **4-bit palette** mode
  (38,400 bytes vs 153,600 at 16-bit), allocated **before** `WiFi.begin()`
  so the block is contiguous; falls back to drawing directly to the panel if
  allocation fails. The UI is restricted to the 15 colors in `COL_BASE`
  (`ui.cpp`) so palette output matches RGB565 pixel-for-pixel (a 4-bit
  palette holds 16 entries — don't exceed that); `uiUsePalette()` remaps
  all `COL_*` constants to palette indices. Night mode swaps the palette
  to red-only luminance values.
- **Settings** (`settings.*`): `gSettings` persisted field-by-field in NVS
  Preferences (namespace `"ticker"`, keys `s.*`), loaded with clamping. Every
  row's field/label-array/option-count/NVS-key/default lives in one
  `ROW_META[ROW_COUNT]` table (`settings.cpp`, keyed by `uint8_t Settings::*`
  member pointers) — `settingsValueLabel()`/`settingsOptionCount()`/
  `settingsOptionLabel()`/`settingsOptionIndex()`/`settingsSet()`/
  `settingsLoad()`/`settingsSaveRow()` are all table lookups/loops over it,
  not per-row switches. `settingsSet()` returns a bitmask of changed rows and
  auto-adjusts invalid candle-size x range combos that would exceed
  `MAX_CANDLES`; the caller persists the changed rows and applies side
  effects (`applySettingChange` in the .ino). The settings UI (`ui.cpp`)
  groups rows under section headers via the `SET_ITEMS` visual model
  (independent of the `ROW_*` enum order); binary rows (On/Off) toggle in
  place without opening the picker, and changing the candle size goes
  through a full-screen confirmation page first because it wipes the candle
  DB. Extending the settings page is the intended cheap extension point —
  add a row enum, a label array, one `ROW_META` entry, and a `SET_ITEMS`
  entry (mirror the same row in `simulator.html`'s own `ROW_META`, which
  uses a field-name string instead of a member pointer).
- **Wi-Fi re-provisioning** (`wifi_creds.*`, `wifi_portal.*`): credentials
  are normally compiled into `config.h`, but `wifi_creds.*` can override that
  from a separate NVS namespace (`"wifi"`, distinct from the `"ticker"`
  settings namespace). Tapping "Forget Wi-Fi network" → CONFIRM clears that
  NVS store, tears down STA, and brings up an open SoftAP
  (`WIFI_PORTAL_SSID`) with a `WebServer`/`DNSServer` captive portal at
  `192.168.4.1` (`Page::WIFI_SETUP`, its own `loop()` short-circuit that
  skips the normal 24/7 job logic — there's no internet in AP mode). The
  key safety property: an aborted setup (Cancel, or power-cycle) leaves no
  NVS credentials, so the *next* boot falls back to `config.h` automatically
  — Forget Wi-Fi network can never hard-lock the device out of every
  network. Only a completed form submission (`POST /save`) writes new NVS
  credentials and reboots onto them.
- **Single-threaded model**: everything runs on the Arduino `loop()` task —
  no locking anywhere; `ui.cpp` reads the candle ring directly between
  mutations. Keep it that way. `loop()` also computes the footer CPU% as an
  EMA of busy time.
- **Simulator parity**: `simulator.html` duplicates firmware constants
  (colors, layout, option tables, timing) and logic in JS. **Any UI or
  settings change in the firmware must be mirrored in `simulator.html`.**

## Code style and conventions

- Language of code, comments, and docs: **English**.
- **Full-width layout**: layout boundaries stop at `CONTENT_RIGHT` (`SCREEN_W`, 320px) with no right-edge padding. Right-aligned text, separator lines, and borders extend to the screen edge. See `ui.cpp` (`drawStatusBar`, `drawChart`, `uiRenderSettings`, `uiDrawTouchFlash`).
- Comment style: dense header comments on every public function explaining
  *why* (hardware gotchas, heap budgets, design rationale), not just what.
  Match this when editing — future maintainers rely on these notes.
- Indentation is inconsistent by history: `btc_ticker.ino` and
  `candles.cpp` use a flush-left-in-block style inherited from a
  donor project, while `store.cpp`, `settings.cpp`, `ui.cpp`, and the
  `net_*` files use normal 2-space indentation. Match the file you're
  editing; don't reformat wholesale.
- Settings page layout lives in `ui.h`/`ui.cpp` and is shared with touch
  hit-testing (`btc_ticker.ino`) so it can't drift: title/gear/confirm
  constants (`UI_SET_TITLE_H`, `UI_GEAR_HIT_*`, `UI_CONFIRM_*`) plus the
  geometry helpers `uiSettingsItemAt()`, `uiSettingsMaxScroll()`, and
  `uiPickerOptionAt()`. Never duplicate that math in the .ino.
- Hardware notes baked into the code: the XPT2046 touch controller is NOT on
  the display's SPI bus (dedicated pins, see `pins.h`); the touch calibration
  in the .ino intentionally swaps `y_min/y_max` to un-mirror screen X at
  rotation=1 — don't "fix" it without testing on-device.
- Make minimal, scoped changes; keep the 24/7 robustness story (backoff,
  supervisors, flash DB) intact when touching main-loop code.

## Testing and verification

There is **no automated test suite**. Verification is:

1. `make build` — must compile cleanly; watch flash usage vs the 3MB
   `huge_app` slot.
2. `make flash && make monitor` — expected serial output: FS mount, ring
   load count, WiFi IP, NTP sync, `backfill: N candles`, 1Hz price lines,
   candle closes at bucket boundaries, heap log every 60s.
3. `simulator.html` in a browser — iterate on UI/settings changes without
   hardware; use its diagnostics panel to exercise corruption/wipe/offline
   paths.
4. On-device robustness checks when touching networking/persistence: pull
   router power (expect backoff + red WiFi dot, gap re-backfill on
   reconnect), power-cycle the board (chart restores from flash, then
   reconciles via backfill).

## Security considerations

- `firmware/btc_ticker/config.h` contains WiFi credentials and is
  **gitignored — never commit it**. Only `config.example.h` (placeholders)
  is versioned.
- All market data comes from Binance's **keyless public REST API** — no API
  keys exist or should be added. (`plan.md` explicitly warns against copying
  a hardcoded CMC API key from a reference project.)
- TLS certificate verification is disabled (`tls.setInsecure()`) in both
  `net_http.cpp` and `net_price.cpp` — a deliberate v1 tradeoff for a
  read-only price ticker; flagged in code comments if that ever changes.
- The Wi-Fi setup portal (`wifi_portal.cpp`) is an **open SoftAP** (no
  password) — same tradeoff class as the TLS point above, chosen so a phone
  can join with one tap. It's only up while the device is deliberately in
  setup mode, on premises the owner controls.
- NVS settings and the LittleFS candle DB are validated/clamped on load
  (corrupted flash or NVS must not crash or poison the UI) — preserve those
  validation paths when editing `settingsLoad()` or `storeInit()`.
