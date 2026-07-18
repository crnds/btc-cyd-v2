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
