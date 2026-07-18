# Future feature ideas for btc-cyd-v2

Idea catalog — nothing here is approved for implementation yet. Pick ideas from
this list and turn them into a concrete implementation plan when ready.

## What's already built (so nothing below duplicates it)

Price + 24h change % from Binance `ticker/24hr` @ configurable cadence · candle
chart with size/range/style pickers (R/G, B/W, line) + chart H/L labels ·
clock/date (NTP, ICT-7; seconds drawn smaller than HH:MM) · **24h range position
bar** (`low ──●── high`, toggleable via Settings → Range bar; uses
`dayHigh`/`dayLow` from the same ticker payload) · WiFi + freshness dots ·
footer CPU/ROM/RAM · settings page w/ option pickers (brightness including
**LDR auto** via GPIO34 when bri index is Auto, 180° flip, intervals, night
schedule/force, range bar on/off) · night mode (red-only + 5% dim 23:00–08:00) ·
LittleFS candle DB + backfill/gap-repair · WiFi supervisor, daily 4AM restart ·
touch (gear, scroll, picker) · `simulator.html` web simulator.

Constraints to respect: tight heap (~45KB per TLS session, 38KB sprite), 320×240,
4-bit palette (~10 colors), 20px right padding (`CLAUDE.md`), keyless REST only,
settings framework (`settings.h/.cpp` + picker pages) is cheap to extend.
Any UI/settings change must stay mirrored in `simulator.html`.

Effort tags: **S** < half a day · **M** ~a day · **L** multi-day.

Design principles for new work:

1. Prefer **draw-only** over new network (ring + ticker payload still have headroom).
2. Keep one primary number, one context strip, one chart — demote something before
   adding a second "big" metric.
3. Settings rows are the cheap extension point; avoid hardcoding toggles.
4. Never break the 24/7 story (backoff, single TLS session, flash DB, night mode).

---

## Product directions (optional coherence, not mutually exclusive)

Pick a personality so features stack cleanly instead of a pile of toggles:

- **Trader glance** — last-price line, countdown, OHLC tap, alerts, multi-symbol;
  market-first footer. Best for daytime desk watching.
- **Living-room ambient** — bigger/softer clock, Fear & Greed / halving, night
  polish, less device stats. Best for 24/7 shelf presence.
- **Bangkok desk clock** — ICT identity, THB under USD, configurable night for
  condo light, LDR auto already fits; optional US/Asia session markers.

Tier-A chart polish below serves all three; directions diverge on Tier B/C picks.

---

## 1. Free data — already fetched (or almost), only needs drawing

1. **24h high/low/volume strip** (**S**) — ticker already supplies `highPrice` /
   `lowPrice` (used by the range bar). Also parse `quoteVolume` and show a dim
   strip under the price row, e.g. `H 122k  L 118k  V $42B`. Zero extra network
   cost. Range bar already covers H/L position; this is the numeric/volume half.
2. **Candle-close countdown** (**S**) — `formingCandle.openEpoch + candleSeconds − now`
   as `"2:31"` in a chart corner (or footer). Makes 5m closes feel alive.
3. **Last-price dashed line + label on chart** (**S**) — horizontal dashed line at
   `lastPrice` via existing `priceToY`. Standard on every real ticker.
4. **Absolute $ change beside %** (**S**) — e.g. `+1.42% · +1,680` from the same
   24h ticker fields (`priceChange` / derived from last and open). No new fetch.
5. ~~**24h range position bar**~~ — **shipped** (toggle in Settings).

### Range-bar upgrades (shipped base)

6. **Range % label** (**S**) — show position in range, e.g. `72%`, next to the bar.
7. **Range-track coloring** (**S**) — tint track or dot by low/mid/high-of-day
   (within palette limits).

---

## 2. Dashboard UX polish (same data, better glance)

8. **Price color / flash by tick direction** (**S**) — tint or briefly flash the
   big price green/red on each move (or for ~1s after a move), not only the `%`.
9. **Forming-candle emphasis** (**S**) — brighter body/outline on the live candle
   so history vs forming is obvious at a glance.
10. **Session open marker on chart** (**S/M**) — dim horizontal or vertical mark
    at midnight ICT (or first visible open) for "where vs start of day".
11. **Time-axis ticks** (**S/M**) — a few dim vertical guides / labels
    (`6h / 12h / now`) under or inside the chart border.
12. **Market-first footer** (**S/M**) — stop always showing CPU/ROM/RAM (engineer-
    facing). Default to freshness text, RSSI, or candle countdown; keep device
    stats behind a "Dev stats" setting or long-press gear.
13. **Degraded-state footer messages** (**S/M**) — when offline/stale, replace
    dots-only with short text: `API 12s stale` / `WiFi reconnect…` /
    `Backfilling 180/288`.
14. **Backfill progress on empty chart** (**S**) — thin bar or `Loading chart…`
    on first boot / after wipe until ring has data.
15. **Gap indication on chart** (**S/M**) — dim break or hatch where the ring has
    missing candles (gap detection already exists for repair).
16. **Header free zone** (**S**) — left of the centered clock is unused; good home
    for symbol (`BTC`), Fear & Greed, or a tiny sparkline without crowding price.

---

## 3. Computed locally from candles already in RAM (no new network)

17. **MA/EMA overlay** (**M**) — EMA-21 (or SMA-20) over visible candles, drawn
    like `STYLE_LINE` in a dim palette color. Pure math over `visibleCandle()`.
18. **Tap-to-inspect crosshair / OHLC** (**M**) — chart area is touch-free today
    (only the gear zone is used). Tap a candle → overlay O/H/L/C + open time;
    tap again to dismiss. Highest "real chart" interaction for no network cost.
19. **Big-move flash** (**S**) — if |Δprice| over the last N fetches exceeds a
    threshold, flash/invert the price row for a second. Settings row for threshold.
20. **RSI-14 mini-readout** (**M**) — one number in footer/stats strip from closes;
    overbought/oversold coloring.
21. **Header sparkline of last N closes** (**S/M**) — tiny line from the ring when
    range bar is off or as an alternate header widget.

---

## 4. Touch & multi-page UX

22. **Long-press chart → cycle style** (**S**) — R/G → B/W → Line without opening
    Settings. Fast path for the existing style setting.
23. **Discoverable gear** (**S**) — brief `SET` label or stronger first-touch
    feedback so the gear is obvious to non-developers.
24. **Swipeable dashboard pages** (**M/L**) — page 2 = full-screen chart, page 3 =
    stats grid (volume strip, funding, F&G, mempool, etc.). Touch layer already
    tracks drags; needs page model + gesture.
25. **Price alerts** (**M**) — high/low thresholds (settings rows, NVS); on breach,
    flashing banner across the chart until tapped away.

---

## 5. New endpoints — Binance, still keyless

26. **Multi-symbol support** (**M**) — settings picker for BTCUSDT/ETHUSDT/SOL…;
    symbol string into `net_price`, `net_klines`, and store filename (per-symbol
    ring files). Pipeline is already symbol-agnostic; header can show the ticker.
27. **Funding rate + countdown** (**M**) — `fapi.binance.com/fapi/v1/premiumIndex`,
    polled every few minutes; small row like `FUND +0.0100% in 3:12`.
28. **THB (or fiat) secondary price** (**M**) — dim conversion under the USD price;
    Binance `BTCTHB` or any fiat-rate API, polled hourly. Strong fit for ICT-7 /
    Bangkok desk-clock direction.

---

## 6. New endpoints — other free APIs

29. **Fear & Greed index** (**M**) — `api.alternative.me/fng/`, hourly; one colored
    number (`GREED 74`) in header/stats area.
30. **On-chain strip from mempool.space** (**M**) — next-block fee (sat/vB), block
    height; keyless lightweight JSON. Network pulse for a 24/7 desk toy.
31. **Halving countdown** (**S** once #30 exists, else **M**) — blocks remaining to
    next halving height × ~10 min → days/date. One line, very on-theme.
32. **US / Asia session markers** (**S/M**) — dim vertical bands or labels for
    major market sessions on the chart time axis (local compute, no API).

---

## 7. Night mode & ambient polish

33. **Configurable night window** (**S**) — start/end hour rows instead of
    hardcoded 23:00/08:00 in `nightModeActive()`.
34. **Timezone picker** (**S/M**) — preset TZ strings in settings instead of
    hardcoded `ICT-7`.
35. **Sunrise brightness ramp** (**S/M**) — e.g. 07:30–08:00 fade from night 5%
    toward user brightness instead of a hard step.
36. **Warm-dim without full red** (**M**) — optional night style that only dims /
    warms within palette limits (living-room ambient path).

---

## 8. Hardware / device

37. ~~**Auto-brightness from the on-board LDR**~~ — **shipped** (Brightness → Auto,
    GPIO34). Revisit thresholds if a board unit mis-reads.
38. **WiFi RSSI in footer** (**S**) — `WiFi.RSSI()` as `-58dBm`; joins or replaces
    CPU/ROM/RAM (pairs well with #12 market-first footer).
39. **OTA updates** (**M**) — ArduinoOTA so USB `make flash` is not required for
    every tweak; esp32 core support, no new libs.
40. **LAN web dashboard** (**L**) — ESP32 web server `/api/state` + small page,
    mDNS `btc-ticker.local`. Heap juggling with TLS keep-alive is the main risk;
    reuse `simulator.html`'s look.
41. **Quiet hours / slower poll** (**S/M**) — optional reduced price cadence at
    night for quieter USB hubs / less heat; orthogonal to visual night mode.
42. **Portrait layout** (**L**) — only if mounting needs it; full layout rewrite.

---

## Short-list recommendation

Best value-per-effort, low risk to the 24/7 robustness story:

**Immediate dashboard payoff (pure UI / free JSON fields):**

- **#3** last-price line, **#2** candle countdown, **#1** H/L/volume strip
- **#8** price tick flash, **#9** forming-candle emphasis
- **#12** + **#38** market-first footer with RSSI (demote always-on CPU/ROM/RAM)

**Interaction unlock:**

- **#18** tap-to-inspect OHLC (chart is dead touch surface today)
- **#19** big-move flash, **#25** price alerts

**Biggest capability / personality jumps:**

- **#26** multi-symbol — largest "new product" step for moderate effort
- **#28** THB secondary price — Bangkok desk-clock identity
- **#29** / **#31** Fear & Greed or halving — ambient personality on the cheap
- **#33** configurable night window — small settings win, high daily comfort

**Defer unless needed:** full LAN web server (#40) heap fight; heavy multi-indicator
packing on a 10-color palette; anything requiring API keys.

### Suggested build order (if doing a "Trader glance" pass)

1. Last-price line + countdown + volume strip  
2. Footer rewrite (freshness / RSSI / countdown; dev stats optional)  
3. Tap candle OHLC  
4. Multi-symbol  
5. Alerts + big-move flash  

### Suggested build order (if doing an "Ambient / Bangkok" pass)

1. Same Tier-A chart polish as above  
2. THB secondary price + header symbol/F&G  
3. Configurable night hours + sunrise ramp  
4. Halving / mempool one-liner  
5. Swipe page for a full stats mosaic  

---

Also note: any UI change must keep `simulator.html` in sync, and follow the
20px right-padding rule in `CLAUDE.md`.
