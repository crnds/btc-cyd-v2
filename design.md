# UI Redesign — btc-cyd-v2 (2026)

Redesign of the CYD firmware UI and its browser simulator. Implemented in
`firmware/btc_ticker/ui.cpp` / `ui.h` (+ touch/navigation in `btc_ticker.ino`)
and mirrored 1:1 in `simulator.html`. All existing functionality is preserved;
only presentation, information architecture, and interaction patterns changed.

## Constraints the design is built around

- 320x240 px ILI9341, no anti-aliasing, GLCD bitmap font (6x8 px per unit
  size). "Typography" on-device means hierarchy via size, color, and tracked
  capitals — not typeface choice.
- 4-bit palette sprite: exactly **16 color slots**; the design uses 15.
- Resistive touch (imprecise): no hover states, generous hit zones, pressed
  feedback must be immediate.
- 1 Hz full-frame re-render on a single thread: every visual must be cheap
  (lines, rects, circles — no alpha blending, no shadows).
- The 20 px right-edge rule (`CONTENT_RIGHT = 300`) is kept everywhere.

## Design system

### Color — "monochrome chassis, signal color only"

Neutral dark ramp carries all structure; color is spent exclusively on
meaning. Every text/background pair used meets WCAG AA contrast (>= 4.5:1)
on the panel fills.

| Token | Value | Role |
|---|---|---|
| `BG` | #000000 | screen background |
| `PANEL` | #101418 | elevation step 1 (chips, pressed picker rows) |
| `PANEL_HI` | #212529 | elevation step 2 (selected/pressed rows, toggle off) |
| `GRID` | #181D21 | chart gridlines, hairlines |
| `BORDER` | #292D31 | control outlines, track lines |
| `TEXT` | #F7F7F7 | primary values |
| `TEXT2` | #9CA2AD | labels, secondary text |
| `TEXT3` | #73828C | micro-labels (4.6:1 on PANEL) |
| `GOOD` / `GOOD_DIM` | #3BDB7B / #187B42 | bull / healthy |
| `BAD` / `BAD_DIM` | #FF5163 / #8B2D3B | bear / error |
| `AMBER` / `AMBER_DIM` | #FFB221 / #5A3D10 | BTC accent, interactive affordances |
| `BW_BULL_DIM` | #7B828C | B/W chart style bull body |

Green and red were also chosen to stay separable for the common
deuteranopia/protanopia deficiencies (green is pushed toward teal,
#3BDB7B vs #FF5163), and no status is ever carried by color alone (see
below). Night mode re-maps the whole palette to red-luminance automatically,
so every new color inherits night behavior for free.

### Typography

One font, four voices:

- **Display** — size 4 (32 px): the price, the only hero element.
- **Title** — size 2 (16 px): clock, page titles, chip text, picker options.
- **Body** — size 1 (8 px): labels, values, dates, axis text.
- **Micro-caps** — size 1 with +1 px tracking, uppercased: section headers
  (`MARKET DATA`, `CHART`, `DISPLAY`), the asset tag (`BTC/USDT`), and the
  feed-status readout (`LIVE` / `STALE 45S` / `OFFLINE`). Tracking turns the
  blocky GLCD font into a deliberate "label tape" voice instead of a
  limitation.

### Spacing & shape

4 px base grid; recurring offsets are 4 / 8 / 12 / 16 / 24. Corner radius is
5–8 px on interactive shapes (chips, buttons, toggles). Hairlines (`GRID`)
separate regions instead of boxes — the dashboard has zero borders around
content, so the data itself is the structure.

---

## Dashboard

### 1. Issues with the old design

- The clock (size 2, centered) competed with the price for top-band
  attention; the date was crammed against it on the right.
- The price row was right-aligned, leaving a dead left half and breaking
  left-to-right reading gravity; the % change sat inline at the same visual
  weight as the currency symbol.
- Status was encoded in two unlabeled dots (blinking green/amber/red) and a
  colored WiFi glyph in the footer — pure color-only signaling.
- CPU/ROM/RAM readouts had equal visual weight with market data.
- Empty/connecting states were a bare `--`; there was no offline or stale
  indication beyond a dot.
- The gear "button" was a bare glyph with no button affordance.

### 2. Design decisions

- Strict top-to-bottom priority: **status → price → 24 h range → chart →
  system footer**. The two things you glance for (price, direction) own the
  upper-left reading start.
- The 24 h change became a filled directional **chip** — the only filled
  element besides toggles — so direction reads at arm's length.
- Connectivity (chrome) went to the status bar; **data freshness** (content)
  went to the footer next to the chart, as labeled text.
- Device stats (CPU/RAM/ROM) were demoted to a single muted micro-line.

### 3. New layout (y-coordinates)

- `0–24` status bar: `BTC/USDT` micro-caps left, clock centered (seconds in
  `TEXT3`), date + WiFi glyph right, hairline below.
- `34–66` price hero: `$` (size 2, `TEXT2`) + digits (size 4), change chip
  right of the baseline (22 px tall, radius 6).
- `72–80` full-width 24 h range bar: low/high labels at the ends, amber
  position dot on a `BORDER` track.
- `92–206` chart (see next section).
- `212–238` footer: hairline, then pulsing dot + `LIVE` / `CONNECTING` /
  `STALE 45S` / `STALE 3M` / `OFFLINE` in micro-caps, then
  `CPU 04%  RAM 19%  ROM 58%` in `TEXT3`; gear in an outlined 28x20 button
  flush with `CONTENT_RIGHT`.

### 4. Visual improvements

Left-aligned hero axis, one filled accent per screen, hairline-separated
regions, consistent 4 px rhythm, and a WiFi glyph scaled to respect the
20 px right-edge rule (radii 3/6/9).

### 5. Interaction improvements

- Freshness thresholds are now **relative to the configured poll interval**
  (`2x interval + 3 s` = live, `4x + 30 s` = stale): at a 5 m cadence the
  old logic showed "stale" permanently.
- The gear button shows a `PANEL_HI` pressed fill while touched (via
  `uiSetPressedPoint`); the global touch flash is now 3 px instead of a
  content-wiping 10 px.
- Gear hit zone unchanged (bottom-right 90x40) — muscle memory preserved.

### 6. Accessibility improvements

Feed state is text + pulsing dot, never color alone; `--` empty states
became explicit `CONNECTING` / `OFFLINE` chips; chart empty state says
`WAITING FOR MARKET FEED` or `OFFLINE — NO CACHED DATA`; all text tokens
>= 4.5:1 contrast.

### 7. Reusable components introduced here

Status bar, change chip, range track, micro-caps label, hairline, icon
button (outlined + pressed fill), labeled status readout.

---

## Chart

### 1. Issues with the old design

- Bare bordered box: no gridlines, no time reference, min/max labels painted
  over candles with no backing (illegible against busy stretches).
- The forming candle was indistinguishable from closed ones.
- No "where is the price now" anchor inside the shape.
- Line style was a naked 2 px stroke with no area context.

### 2. Design decisions

- Reference lines at **min / mid / max** in `GRID` — the three values that
  actually orient you, drawn first so data always sits on top.
- Y-labels as **BG-backed tags** inside the left edge: readable over any
  candle pattern without reserving a permanent gutter (which the 288 px plot
  width can't afford — 288 candles at >= 1 px each).
- A dashed `AMBER_DIM` **last-price line** with an amber right-edge tag —
  the single most useful chart overlay for a ticker.
- A real x-axis strip: tick every 2 h (12 h view), 4 h (24 h), or daily
  (7D), labels in `TEXT3` below the plot so they never fight the candles.

### 3. New layout

Plot `y 92–189` (288 px wide, right edge at `CONTENT_RIGHT - 2`), axis
hairline at `y 190`, tick labels `y 196–204`.

### 4. Visual improvements

Hollow (outline-only) forming candle in Red/Green style — "still live"
without a legend; dim amber area fill under the Line style; gridlines and
axis in the quietest ramp color so candles keep rank.

### 5. Interaction improvements

Style/interval/range changes flow from settings exactly as before; the
chart tolerates all three styles + 12h/24h/7D ranges with the same axis
logic.

### 6. Accessibility improvements

Labels always legible (backing), forming-vs-closed is a shape difference
(not color), gridline/label contrast >= 4.5:1, day-of-month labels on 7D
prevent time-zone ambiguity.

### 7. Reusable components

BG-backed text tag, dashed reference line, time-tick axis — all candidates
for any future chart (e.g., a second asset).

---

## Settings (list, picker, confirmation)

### 1. Issues with the old design

- Nine flat rows mixing unrelated domains; no scanning structure.
- Every row opened a sub-page — even On/Off toggles (two taps for one bit).
- No descriptions anywhere; `Night mode active` actually meant "force on".
- Changing the candle size **wiped the flash DB with zero confirmation** —
  the one destructive action in the product.
- The back hit-zone (x < 90 in the title bar) was invisible convention.
- Selected picker rows were marked by a 6 px radio only.

### 2. Design decisions

- **Categorization instead of search.** A keyboard-less 320x240 resistive
  display can't host search; three groups (`MARKET DATA`, `CHART`,
  `DISPLAY`) get the same findability with zero input cost.
- **Binary rows are toggles.** Four On/Off settings flip in place with a
  switch glyph — one tap, no navigation. Multi-choice rows keep the picker.
- **Descriptions live on the picker page**, one line under the title, where
  the user is about to commit to a choice; the list stays lean.
- **Destructive action gets a confirmation page** (see below).

### 3. New layout

- List: 30 px title bar (`< Settings`), then grouped items — 26 px section
  headers (micro-caps + hairline) and 40 px rows (label `TEXT2` left, value
  `TEXT` right + amber chevron, or a 30x16 toggle). 438 px of content,
  drag-scrolled under the clipped title bar with a 2 px scrollbar thumb.
- Picker: title = setting name, description at `y 38`, options from `y 52`
  (40 px rows, shrinking to fit when > 4). Selected row: full-width
  `PANEL_HI` fill + amber radio; pressed row: `PANEL` fill.
- Confirm (full-screen, modal): warning glyph, `CLEAR CHART HISTORY?`,
  two-line explanation, the pending change rendered as `5m -> 15m` in
  amber, then `CANCEL` (outline) / `CONFIRM` (amber fill) 120x34 buttons.

### 4. Visual improvements

Section headers create chunking (Miller's 7±2 → 3 groups); values use the
primary color because the value — not the label — is the information;
toggles match the simulator shell's switch style one-to-one.

### 5. Interaction improvements

One-tap toggles; re-tapping the current option just closes the picker;
the confirmation page is modal (taps outside the buttons do nothing) and
its hit boxes are shared constants (`UI_CONFIRM_*`); pressed-state
highlights on every tappable row; all layout math is shared between
rendering and hit-testing via `uiSettingsItemAt()` / `uiSettingsMaxScroll()`
/ `uiPickerOptionAt()` so it cannot drift.

### 6. Accessibility improvements

40 px row targets (maximizing what 240 px allows), persistent selection
fill + radio (shape + color), plain-language descriptions, explicit
consequence text for the destructive path, and `Force night mode` renamed
from the ambiguous `Night mode active`.

### 7. Reusable components

Section header, settings row (label/value/chevron), toggle switch, radio
row, description line, confirmation page with button pair — the picker and
dialog patterns cover any future setting without new code.

---

## Navigation & information architecture

- `Dashboard ⇄ Settings list ⇄ Picker → Confirm` — four levels, one
  gesture each, every level with a visible `<` return in the title bar
  (left 90 px is the back zone, unchanged).
- Page state in the .ino gained exactly one variable (`confirmIdx`) — the
  confirm page is a property of the picker, not a new top-level page.
- The simulator mirrors the same state machine, so every flow can be
  exercised without hardware.

## Deliberately not done (with rationale)

- **Custom typeface / anti-aliased text** — requires bundling smooth fonts
  (flash + rendering cost) and breaks the simulator's pixel parity; the
  tracked-caps voice achieves the modern look within the GLCD font.
- **Search in settings** — no keyboard on a resistive 320x240 panel;
  categorization solves the same problem.
- **Shadows / translucency** — impossible on a 4-bit palette with no alpha;
  elevation is expressed with `PANEL` / `PANEL_HI` fills instead.
- **Moving device stats off the dashboard** — kept (demoted) because the
  24/7 robustness story makes them operationally useful.

## Verification

- `make build`: compiles clean, 38% of the 3 MB `huge_app` slot.
- `simulator.html`: rendered headless in Chrome — dashboard (live Binance
  data), settings list, picker, and confirm page all verified visually;
  all element ids referenced from JS exist; boot log sequence verified
  end-to-end (WiFi → NTP → backfill of 288 candles).
- Fixed in passing: the simulator's `settingsSet()` rejected row 8
  (`row >= 8` instead of `row >= ROW_COUNT`), so "Range bar" could never be
  changed in the sim; boot log lines now claim the 4-bit palette buffer the
  firmware actually uses; multi-line log template no longer triple-wraps
  console entries.
