# btc-cyd-v2

24/7 BTC/USDT ticker, clock, and 24h candlestick dashboard for the CYD board
(ESP32-2432S028R). See `plan.md` for the full design.

## Build

Requires `arduino-cli` with esp32 core 3.3.10, LovyanGFX 1.2.25, ArduinoJson 7.4.3.

```sh
cp firmware/btc_ticker/config.example.h firmware/btc_ticker/config.h
# edit config.h with your WiFi SSID/password

make build     # compile
make flash      # compile + upload to /dev/cu.usbserial-110
make monitor    # serial monitor @ 115200
```

## Layout

```
firmware/btc_ticker/
├── btc_ticker.ino   # LGFX class, sprite, setup()/loop(), scheduler, touch
├── pins.h           # CYD board pin mapping
├── config.h         # WiFi creds (gitignored)
├── candles.h/.cpp   # candle assembly, RAM ring, gap repair
├── store.h/.cpp     # LittleFS ring-file persistence (/candles.bin)
├── net_http.h/.cpp  # HTTP/1.0 one-shot helper
├── net_price.h/.cpp # keep-alive TLS 1Hz price fetch
├── net_klines.h/.cpp# streaming klines backfill parser
└── ui.h/.cpp        # sprite drawing
```

## Simulator

The project includes `simulator.html`, a high-fidelity ESP32 CYD hardware simulator that runs entirely in the browser. It features:
- **Live Market Feed**: Fetches real-time market price updates and backfills candle histories using the Binance API.
- **Hardware Simulation**: Simulates the LovyanGFX display engine, touch digitizer coordinate maps, and local NVS settings.
- **Interactive Settings Page**: Tap the gear icon in the footer to access Settings, drag-scroll the rows, change chart styles (Red/Green, Black/White, Line), screen rotation (flip), and adjust brightness.
- **Diagnostics Dashboard**: Configurable network status controls (WiFi router connection, API availability), local LittleFS storage sector corruption tools, database wipers, and real-time serial logs console.

To run, simply open `simulator.html` in any modern web browser.

