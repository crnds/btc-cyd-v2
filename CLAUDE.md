# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**Read `AGENTS.md` first.** It is the authoritative, actively-maintained guide for this repo: project
overview, tech stack, build/flash/monitor commands, full repository layout, module-by-module design
notes (candle engine, flash store, networking, display, settings), code style, testing/verification
steps, and security considerations. Everything below is a thin index on top of it — don't duplicate
it here; update `AGENTS.md` when conventions change.

## Quick reference

```sh
cp firmware/btc_ticker/config.example.h firmware/btc_ticker/config.h  # one-time, gitignored
make build     # arduino-cli compile
make flash     # compile + upload to /dev/cu.usbserial-110
make monitor   # serial monitor @ 115200
make clean     # rm -rf firmware/btc_ticker/build
```

No automated test suite — verification is `make build` + on-device/simulator checks (see
"Testing and verification" in `AGENTS.md`).

## Things easy to forget

- **`simulator.html` must mirror any firmware UI/settings change.** It's a from-scratch JS
  reimplementation of `ui.cpp`/`btc_ticker.ino`, not generated — colors, layout constants, option
  tables, and state machine all need to be hand-kept in sync.
- Two indentation styles coexist by history (`btc_ticker.ino`/`candles.cpp` flush-left-in-block vs.
  2-space elsewhere) — match the file you're editing, don't reformat.
- `config.h` is gitignored and must never be committed; only `config.example.h` is versioned.
- Only one TLS session may be open at a time (heap budget) — see the networking notes in
  `AGENTS.md` before touching `net_price.cpp`/`net_klines.cpp`.
- Settings-page layout constants are shared between drawing (`ui.cpp`) and touch hit-testing
  (`btc_ticker.ino`) — never duplicate that geometry math.

## Other docs in this repo

- `design.md` — UI/visual design system rationale (colors, typography, layout decisions) for the
  2026 redesign; read before changing anything user-facing.
- `plan.md` — original architecture/design rationale and verification plan.
- `future-plan.md` — idea catalog for possible future features; **not approved work**, do not
  implement from it without being asked.
