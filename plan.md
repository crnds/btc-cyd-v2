# btc-cyd-v2 — BTC Ticker/Clock/Candlestick Dashboard Firmware

## Context

Build new firmware for the connected CYD board (ESP32-2432S028R: ESP32-D0WD-V3, 4MB flash, 320x240 ILI9341 TFT, XPT2046 touch, on `/dev/cu.usbserial-110`). It's a 24/7 Bitcoin ticker + clock dashboard:

- BTC/USDT price from Binance REST, polled every **1 second**
- **288 five-minute candles (24h)** rendered as 1px-wide candlestick columns
- **Mini-database in flash**: rolling 24h candle log surviving reboots/power loss
- Clock via NTP (Bangkok ICT-7)

**User decisions:** 1px candles full 24h · arduino-cli build · WiFi creds in gitignored config.h.

Two prior projects supply proven code: `/Users/dusitn/cyd-stripdown` (LovyanGFX display config, sprite double-buffering, arduino-cli workflow) and `/Users/dusitn/btcticker-cyd` (Binance TLS networking, job scheduler, NTP). All required libs already installed: esp32 core 3.3.10, LovyanGFX 1.2.25, ArduinoJson 7.4.3.

## Project structure

```
/Users/dusitn/btc-cyd-v2/
├── Makefile                      # build / flash / monitor
├── .gitignore                    # firmware/btc_ticker/config.h
├── README.md
└── firmware/btc_ticker/          # arduino-cli sketch dir
    ├── btc_ticker.ino            # LGFX class, sprite, setup()/loop(), scheduler, touch
    ├── pins.h                    # from cyd-stripdown pins.h
    ├── config.example.h          # template; config.h gitignored (copy creds from
    │                             #   cyd-stripdown/firmware/cyd_dashboard/config.h)
    ├── candles.h/.cpp            # CandleRec, RAM ring[288], assembly/close/gap logic
    ├── store.h/.cpp              # LittleFS ring-file persistence
    ├── net_http.h/.cpp           # HTTP/1.0 one-shot helper
    ├── net_price.h/.cpp          # keep-alive TLS 1Hz price fetch
    ├── net_klines.h/.cpp         # streaming klines backfill parser
    └── ui.h/.cpp                 # all sprite drawing
```

**Build config:** FQBN `esp32:esp32:esp32:PartitionScheme=huge_app,UploadSpeed=115200` (verified in boards.txt: 3MB app / 1MB FS — needed since WiFi+TLS+LovyanGFX exceeds the default 1.2MB app slot). Makefile targets: `build`, `flash`, `monitor` (baudrate 115200, port `/dev/cu.usbserial-110`).

## Code to borrow (verified locations)

| What | Source |
|---|---|
| LGFX display+touch class (ILI9341 VSPI sclk14/mosi13/miso12/dc2/cs15, BL 21 PWM, XPT2046 on 25/32/39/33/36) | `cyd-stripdown/firmware/cyd_dashboard/cyd_dashboard.ino:18-74` — verbatim |
| Full-frame sprite double-buffer w/ 16→8-bit fallback | same file `:76-83, 514-527` |
| Touch debounce pattern | same file `:583-591` |
| Keep-alive TLS 1Hz price fetch (`fetchPrice`) | `btcticker-cyd/src/net/price.cpp` — verbatim (uses ArduinoJson filter for `lastPrice`/`priceChangePercent`) |
| HTTP/1.0 one-shot helper (`useHTTP10` unchunked) | `btcticker-cyd/src/net/http.cpp` — verbatim |
| Streaming bracket-depth klines parser | `btcticker-cyd/src/net/cdc.cpp:12-56` — adapt (see below) |
| Job scheduler + exponential backoff | `btcticker-cyd/src/main.cpp:96-141` |
| Daily 4AM restart | `btcticker-cyd/src/main.cpp:316-323` |
| NTP: `configTzTime("ICT-7", "pool.ntp.org")` | `btcticker-cyd/src/main.cpp:374` |

Do NOT copy: the CMC API key hardcoded in `btcticker-cyd/include/config.h:22`.

## Key designs

### Flash mini-database — LittleFS ring file `/candles.bin`
- 288 × 20-byte packed records, **no header, self-indexing**:
  ```cpp
  struct __attribute__((packed)) CandleRec {  // 20 bytes
    uint32_t openEpoch;  // UTC sec, % 300 == 0
    float o, h, l, c;
  };
  // slot = (openEpoch / 300) % 288  — slot is a pure function of time
  ```
- Write on candle close (`seek(slot*20)` + 20-byte write); also persist forming candle every 60s (bounds loss to <1min). ~576 small writes/day on wear-leveled 1MB FS ≈ decades.
- Boot load: read all 288, validate each (`epoch%300==0`, within last 24h, `l<=min(o,c)`, `h>=max(o,c)`, sane price range); invalid → gap. Power-loss torn writes are caught by validation.
- `LittleFS.begin(true)` formatOnFail; wrong file size → recreate zero-filled.

### Candle assembly (candles.cpp)
- Bucket = `time(nullptr) / 300`. Per 1s tick: same bucket → `h=max, l=min, c=p`; new bucket → close forming candle (RAM ring + flash), start new with `o=price`.
- Boundary check runs every loop pass (not only on ticks) so a candle closes even if WiFi is down.
- **Boot**: mount FS → load ring → paint chart immediately → WiFi → NTP (block candle writes until `time() > 1.7e9`) → backfill.
- **Backfill** (`GET /api/v3/klines?symbol=BTCUSDT&interval=5m&limit=288`): elements 0..286 are closed candles (authoritative, overwrite ring+flash); element 287 (in-progress) seeds the forming candle. Handles boot-mid-candle.
- **Gap repair**: after reconnect, if `now/300` minus newest closed bucket > 1, re-run backfill with `limit = min(288, gap+1)`. Slot-by-time makes out-of-order writes harmless.

### Klines parser adaptation (only nontrivial port)
Extend cdc.cpp's bracket-depth scanner: at depth 2, capture field 0 (openTime, bare number, ms→s) plus quoted fields 1-4 (o,h,l,c); on each `]` at depth 2 emit a CandleRec via callback. Zero-buffering — payload is ~90KB and must stream over the HTTP/1.0 unchunked connection.

### Screen layout (320x240 landscape, 1Hz full sprite render)
```
y   0..26   HH:MM:SS clock + date | status dots right: WiFi (grn/red),
            data freshness (grn / amber >5s / red >30s)
y  28..80   Big price "$118,432" + 24h change "+1.42%" (grn/red) right-aligned
y  82..214  Chart: plot x=16..303 (288 cols), y=84..212 (128px)
            per candle: 1px column — wick l..h dim grn/red, body o..c bright
            (min 1px); forming candle live-updates; H/L labels overlaid dim
y 216..239  Footer: activity status (CPU, ROM, RAM %) (dim, size-1)
```
Heap note: allocate the sprite **before** `WiFi.begin()` (16-bit needs 150KB contiguous); keep cyd-stripdown's 8-bit fallback.

### 24/7 robustness
- Price endpoint: `/api/v3/ticker/24hr?symbol=BTCUSDT` (price + change% in one call; weight 2 × 1Hz = 120/min vs 6000/min limit — fine).
- Jobs: `price` 1000ms · `backfill` on-demand · `ntp` resync 6h · `heaplog` 60s serial.
- WiFi supervisor: `setAutoReconnect(true)`; disconnected >60s → re-`begin()`; >10min → `esp_restart()`.
- Daily 4AM ICT restart (uptime guard ≥1h) — safe, flash DB + backfill repair the boot.
- Touch: tap cycles backlight 230→120→40→8 via `gfx.setBrightness()`, step persisted in NVS Preferences (single uint8).

## Implementation order

1. **Scaffold**: Makefile, .gitignore, pins.h, config.example.h+config.h, .ino with LGFX + sprite + hello render → `make build && make flash`, screen lights.
2. **WiFi + NTP + clock UI** → live clock on screen, IP in serial.
3. **Price path**: net_http, net_price, scheduler, price row UI → 1Hz updates; pull WiFi to test backoff/recovery.
4. **Candle engine (RAM)**: assembly + boundary close + chart render → forming candle visible, closes logged at :00/:05.
5. **Backfill**: net_klines + boot/gap jobs → full 288-candle chart seconds after boot; spot-check vs Binance.
6. **Persistence**: store.cpp → reboot paints chart instantly pre-WiFi; power-cycle loses <1min.
7. **Robustness + touch**: daily restart, WiFi supervisor, heap log, brightness cycle.

## Verification

```sh
make build     # flash usage vs 3MB
make flash     # to /dev/cu.usbserial-110
make monitor   # expect: FS mount, ring load count, WiFi IP, NTP, "backfill: 288",
               # 1Hz price lines, candle closes at :00/:05, heap every 60s
```
- Pull router power → backoff + red dot; reconnect → gap re-backfill.
- Power-cycle board → chart restores from flash, then reconciles via backfill.
- Overnight soak → 4AM restart in serial scrollback, 24h contiguous candles, stable heap.
