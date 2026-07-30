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
- The layout runs full width (`CONTENT_RIGHT = 320`) with no right-edge padding.

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
  (`GLOBAL`, `BTC TICKER`), the asset tag (`BTC/USDT`), and the feed-status
  readout (`LIVE` / `STALE 45S` / `OFFLINE`). Tracking turns the blocky GLCD
  font into a deliberate "label tape" voice instead of a limitation.

### Spacing & shape

4 px base grid; recurring offsets are 4 / 8 / 12 / 16 / 24. Corner radius is
5–8 px on interactive shapes (chips, buttons, toggles). Hairlines (`GRID`)
separate regions instead of boxes — the dashboard has zero borders around
content, so the data itself is the structure.

---

## HOME (app launcher)

### 1. Issues with the old design

There was no home screen. Dashboard was the implicit landing page, and every
other surface (Settings, the ambient clock) was reached and left through
page-specific chrome scattered across the UI: a footer gear, a status-bar
clock tap, a settings `<` back zone, a clock "X". No single mental model,
and no way to "go back" from anywhere.

### 2. Design decisions

- **iPad-style launcher, one control out.** `Page::HOME` is a 4-column x
  3-row grid of square app tiles — the new default landing page and the
  restorable "home base." Apps are entered by tapping a tile; the *only* way
  to leave an app is the **Home button in the shared footer** (right side,
  next to the feed pulse/wifi/device-stats readout on the left) — a tap
  jumps straight to HOME. This replaces every prior exit affordance,
  including an earlier long-press-anywhere gesture that shipped for one
  iteration and was dropped for a visible, discoverable control: the
  dashboard gear, the status-bar clock tap, the clock's "X," and the
  settings list's `<` back zone are all gone — Settings/CLOCK entry now
  happens only from a HOME tile, and the settings **picker**'s `<` (picker →
  list) is the only back-chevron left, since that's in-app hierarchy, not
  app exit.
- **Grid sized for growth, not just today's 4 apps.** 4x3 = 12 slots, filled
  left-to-right/top-to-bottom; only the first 4 are populated (BTC TICKER,
  CLOCK, Settings, Weather). Future apps drop into the next free slot with
  no layout change.
- **Tiles reuse existing vector icons**, not a new bitmap system: BTC TICKER
  gets the boot splash's "B-in-circle" brand glyph, CLOCK gets a static
  `drawAnalogFace()` fixed at 10:10 (the same component the full-screen
  clock uses, just at icon radius, but not live — see "Weather" below for
  why HOME tiles don't tick), Settings keeps the familiar gear glyph —
  relocated from the old footer button onto its own tile — and Weather gets
  `drawWeatherIcon()`'s sun glyph (a fixed clear-sky condition regardless of
  live conditions, since real data hasn't loaded yet on a cold tile). Tile
  chrome is the same `drawButtonChrome()` shape used everywhere else
  (outline + `PANEL_HI` pressed fill), so it costs nothing new in the
  palette or render budget.

### 3. New layout

Square tiles (70x70, 6px gutters) centered on the 320x240 screen — no title
bar, matching the full-bleed dashboard/clock pages. Each tile: rounded
`PANEL` card (pressed → `PANEL_HI`), centered icon, `drawCaps` micro-label
caption below (`BTC` / `CLOCK` / `SETTINGS` / `WEATHER`).

### 4. Visual improvements

A single consistent "icon + caption" vocabulary for launching anything,
instead of three unrelated chrome conventions (gear, clock-tap, back-zone)
that only existed because there was no home screen to unify them under.

### 5. Interaction improvements

Tile hit-testing (`uiHomeTileAt()`) and drawing (`uiRenderHome()`) share one
grid-math source of truth, exactly like the settings list's
`uiSettingsItemAt()`/`uiSettingsMaxScroll()` pattern — they can't drift
apart. The footer Home button's hit box (`UI_FOOTER_HOME_*` in `ui.h`) is
the same single source of truth shared between drawing (`uiDrawFooter()`)
and hit-testing (`handleTap()`), checked ahead of every page's own tap
handling so e.g. the clock's whole-screen tap-to-cycle doesn't also fire
when the tap actually lands on the Home button. A settings-list scroll-drag
is explicitly exempted (only a tap, not a drag, dispatches), so scrolling
can't accidentally bounce you home.

### 6. Accessibility improvements

One universal, muscle-memorable, and now **visible** control to get home
from anywhere — no hunting for a back zone or a differently-styled exit
control per page, and no hidden gesture to discover either. (An earlier
iteration used a 3s long-press-anywhere gesture instead; it worked but had
no on-screen affordance at all, so it was replaced with the footer Home
button once the footer existed to hold it.)

### 7. Reusable components

Square tile card (`drawButtonChrome` + centered icon + caption) — the
pattern for any future app tile; the footer Home button itself
(`drawButtonChrome` + a small house glyph, `drawHomeIcon()`) — the same
button chrome and hit-testing pattern reused everywhere the shared footer
appears.

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
  `CPU 04%  RAM 19%  ROM 58%` in `TEXT3`, then a Home button flush right
  against `CONTENT_RIGHT`. (The gear button that used to sit in that same
  right-side spot moved to a HOME tile in the iPad-launcher redesign — see
  "HOME (app launcher)" above — and the Home button itself is a later
  addition once long-press-to-home was retired.) This same footer
  (`uiDrawFooter()`) is now shared by every full-screen page — Clock,
  Weather, HOME, and the top-level Settings list all end their render with
  it, not just the dashboard.

### 4. Visual improvements

Left-aligned hero axis, one filled accent per screen, hairline-separated
regions, consistent 4 px rhythm, and a WiFi glyph (radii 3/6/9).

### 5. Interaction improvements

- Freshness thresholds are now **relative to the configured poll interval**
  (`2x interval + 3 s` = live, `4x + 30 s` = stale): at a 5 m cadence the
  old logic showed "stale" permanently.
- The global touch flash is now 3 px instead of a content-wiping 10 px.
- (The gear button's own pressed-fill/hit-zone notes below are superseded —
  see "HOME (app launcher)": the dashboard's only tap target of its own is
  now the footer Home button, shared with every other full-screen page.)

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
  display can't host search; grouping gets the same findability with zero
  input cost. Originally three data-domain groups (`MARKET DATA`, `CHART`,
  `DISPLAY`, plus a `NETWORK` group for the Wi-Fi reset row); the later
  iPad-launcher redesign regrouped by **scope** instead — `GLOBAL` (applies
  regardless of which app is open: brightness, flip, night mode, and the
  Wi-Fi reset all live here now) and `BTC TICKER` (everything that only
  affects that app, including its own status-bar Clock/Date toggles). CLOCK
  gets no section yet since it has no settings; add one the day it does.
- **Binary rows are toggles.** Four On/Off settings flip in place with a
  switch glyph — one tap, no navigation. Multi-choice rows keep the picker.
- **Descriptions live on the picker page**, one line under the title, where
  the user is about to commit to a choice; the list stays lean.
- **Destructive action gets a confirmation page** (see below).

### 3. New layout

- List: 30 px title bar (plain `Settings` label, no back chevron — see
  "HOME (app launcher)" above for why: exit is the footer Home button),
  then grouped items — 26 px section headers (micro-caps + hairline) and
  40 px rows (label `TEXT2` left, value `TEXT` right + amber chevron, or a
  30x16 toggle), drag-scrolled under the clipped title bar with a 2 px
  scrollbar thumb. The list now shares the same footer as every other
  full-screen page (see "Dashboard" above), so its scrollable area stops at
  `CONTENT_BOTTOM` instead of the screen edge — the picker and confirmation
  pages below don't draw the footer and keep using the full height.
- Picker: title = setting name, description at `y 38`, options from `y 52`
  (40 px rows, shrinking to fit when > 4). Selected row: full-width
  `PANEL_HI` fill + amber radio; pressed row: `PANEL` fill.
- Confirm (full-screen, modal): warning glyph, `CLEAR CHART HISTORY?`,
  two-line explanation, the pending change rendered as `5m -> 15m` in
  amber, then `CANCEL` (outline) / `CONFIRM` (amber fill) 120x34 buttons.

### 4. Visual improvements

Section headers create chunking; values use the primary color because the
value — not the label — is the information; toggles match the simulator
shell's switch style one-to-one.

### 5. Interaction improvements

One-tap toggles; re-tapping the current option just closes the picker;
the confirmation page is modal (taps outside the buttons do nothing) and
its hit boxes are shared constants (`UI_CONFIRM_*`); pressed-state
highlights on every tappable row; all layout math is shared between
rendering and hit-testing via `uiSettingsItemAt()` / `uiSettingsMaxScroll()`
/ `uiPickerOptionAt()` so it cannot drift. The list page's footer Home
button (`UI_FOOTER_HOME_*`) is checked ahead of row hit-testing in
`handleTap()`, and `uiSettingsItemAt()` explicitly excludes taps at or below
`CONTENT_BOTTOM` so a scrolled-into-view row can never shadow it.

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

## Wi-Fi setup ("Find Access Mode")

### 1. Problem

Wi-Fi credentials were compile-time-only (`config.h`), so switching networks
meant editing that file and reflashing over USB — fine for the original
owner at first setup, not something a phone can do later.

### 2. Design decisions

- **A destructive-action row, not a form.** The device has no keyboard —
  credential entry has to happen on the phone's own keyboard, not this
  resistive panel. So the on-device side is just one Settings row,
  "Forget Wi-Fi network" (NETWORK section, bottom of the list, label in
  `BAD` instead of the usual `TEXT2` — the same color-plus-text pairing used
  everywhere else, never color alone), reusing the existing confirmation-page
  pattern (candle size already established: destructive settings row → full
  screen "are you sure" → CONFIRM/CANCEL) rather than inventing a new one.
- **Open SoftAP, no scan dropdown.** The phone joins an open network
  (`BTC-Ticker-Setup`) and types the target SSID/password into a plain HTML
  form — no live `WiFi.scanNetworks()` list. Typing a network name on a
  phone keyboard is normal for this device class (same pattern as
  Tasmota/Shelly-style setup portals); skipping the scan keeps the new
  surface area small. The open-AP tradeoff is documented alongside the
  existing `tls.setInsecure()` one in `AGENTS.md`.
- **Fallback-on-abort, not test-before-commit.** Rather than trying to
  validate new credentials before switching to them (which needs concurrent
  AP+STA and a rollback timer), "Forget Wi-Fi network" simply clears the
  stored credentials immediately. If setup is ever abandoned — Cancel, or a
  power cycle — the *next* boot finds no stored credentials and falls back
  to the compiled `config.h` network automatically. A typo'd password after
  a completed submission is still recoverable: the dashboard and Settings
  stay fully touch-usable even fully offline, so the owner just runs Forget
  Wi-Fi network again.

### 3. New layout

Setup page (full screen, entered from Settings via the confirm page):
`SETUP MODE` title, `CONNECT YOUR PHONE TO` micro-caps + the AP SSID,
`THEN OPEN` micro-caps + `http://192.168.4.1`, a hairline, a one-line note,
and a single outlined CANCEL button (same pressed-fill pattern as every
other icon/text button in the app) — reboots immediately, which is always
safe per the fallback model above.

### 4. Accessibility improvements

No data entry happens on the resistive panel at all — the phone's own
keyboard and screen reader support carry that load. The setup page states
plainly what's about to happen and how to back out, rather than leaving the
device in a silent, unlabeled AP-mode limbo.

### 5. Reusable components

None new — this page composes the existing confirmation-page pattern and
the existing outlined/pressed-fill button component from the clock's close
control and the confirm page's own buttons.

## Full-screen clock

### 1. Issues with the old design

- The status-bar clock was glanceable but not ambient: living-room use
  still required the full BTC dashboard chrome.
- No dedicated clock presentation existed despite the product being a
  24/7 desk clock as much as a ticker.

### 2. Design decisions

- **Enter from the status-bar clock.** The centered time is already the
  clock affordance; tapping it opens a full-screen ambient page. The
  left pulse/wifi and right date stay non-interactive so the hit zone
  stays unambiguous.
- **Three faces, one gesture.** Default is a 50/50 split (minimal analog
  left, 24h digital right). Whole-screen tap cycles split → big analog →
  big digital → split. Mode is remembered across exits so re-entry is
  sticky.
- **Minimal analog, design-system colors.** White face circle + tapered
  white hour/minute hands + thin dual-accent second hand (coral body,
  green tip) on pure `BG`. No ticks, no numerals — matches the reference
  face and stays within the 15-color palette / no-AA budget.
- **24h digital, no seconds.** Split pane stacks `HH` over `MM` (no
  colon) to fill the half-width column; digital-only uses a single
  row `HH:MM`. Both auto-pick the largest GLCD `textSize` that fits.
- **Visible exit (superseded twice).** This originally shipped with an
  outlined amber "X" icon button top-left (same pressed-fill pattern as the
  gear), not a hidden back-zone convention. The iPad-launcher redesign
  removed it in favor of a long-press-anywhere gesture; that in turn was
  replaced by the shared footer's Home button once the footer existed to
  hold a visible control again. The clock page has no in-page controls of
  its own beyond the whole-screen mode-cycle tap and the footer Home
  button — see "HOME (app launcher)" above.

### 3. New layout

- Split: vertical `GRID` hairline at x=160; analog face centered in the
  left half (r≈68) with margin; digital `HH`/`MM` stacked at size ≤8
  in the right half so neither pane feels bloated.
- Analog-only and split: face is centered with no offset (the +4 y offset
  once reserved for the X button was removed along with it).
- Digital-only: single-row `HH:MM`, max-scaled to the full 320 width
  (~size 10 — width-bound by 5 glyphs).
- All three faces are now sized/centered against `CONTENT_BOTTOM` (212),
  not the full 240 px screen height, so the shared footer below them always
  has room — the analog-only face shrank from r=92 to r=78 to make space;
  the digital-only box's height shrank too, but the layout stays
  width-bound so the visible size is unchanged.

### 4. Visual improvements

Pure black field, one hairline fold in split mode, amber second hand for
motion without adding palette entries.

**Night mode:** the clock page has no private palette. Every pixel uses
`COL_*` tokens (`BG`, `TEXT`, `TEXT3`, `AMBER`, `BORDER`, `PANEL_HI`,
`GRID`). `renderIfDue` always calls `uiSetNightMode()` before
`uiRenderClock`, so the face, ticks, hands, digital digits, and split
hairline all remap to red-luminance automatically — same fully-red UI as
the dashboard, settings, and HOME.

### 5. Interaction improvements

Whole-screen tap cycles split → analog → digital → split; no drag on this
page. Exit is the footer Home button, checked ahead of the mode-cycle tap
so a tap landing there doesn't also cycle the face — see "HOME (app
launcher)."

### 6. Accessibility improvements

24h format removes AM/PM ambiguity; stacked HH/MM at max size keeps the time
readable at arm's length.

### 7. Reusable components

Analog face (circle + tapered hands + hub), digital time block — the face
is also reused as the CLOCK tile's icon on HOME, but static (fixed at
10:10) rather than live, since it's an app icon, not a second clock.

---

## Weather

### 1. Issues with the old design

There was no ambient weather glance — a desk clock/ticker with a "Weather"
tile sitting unused for a future app was the only gap left once HOME shipped
with room for growth.

### 2. Design decisions

- **Fourth HOME tile, same launcher vocabulary.** Weather is entered like
  every other app (tap the HOME tile), and left the same way (the shared
  footer's Home button). No new navigation concept, just a new full-screen
  page (`Page::WEATHER`) alongside Dashboard/Clock/Settings.
- **Fixed location, no on-device picker.** Latitude/longitude/city are
  compiled into `config.h` (`WEATHER_LAT`/`WEATHER_LON`/`WEATHER_CITY`),
  same philosophy as the Wi-Fi credentials — no keyboard input on a
  touchscreen this small.
- **Keyless public API, translated to the existing icon taxonomy.**
  Open-Meteo's forecast API needs no signup/appid, matching the Binance
  price/klines philosophy. Its WMO weather codes are translated
  (`wmoToCond()`) into the same condition-id ranges `drawWeatherIcon()`
  already switches on, so the drawing code stays provider-agnostic.
- **Three data tiers, one screen.** Current conditions + today's hi/lo
  (top), the next `WEATHER_NUM_HOURS` hours as a horizontal strip (middle),
  and `WEATHER_NUM_DAYS` days as rows with an iOS-style range bar scaled to
  the week's min/max (bottom) — the same range-bar component the dashboard
  uses for 24h price range, reused at forecast scale.
- **Cold-tile icon, not live data.** The HOME tile itself always shows a
  fixed clear-sky sun glyph (`drawWeatherIcon(..., 800)`) rather than the
  real current condition — a first render happens before the first fetch
  completes, and a stale/wrong icon would be worse than a neutral one.

### 3. New layout

Top: city caption (top-left), "updated Nm ago" caption (top-right, driven
by last-successful-fetch time, independent of whether the *current* poll
succeeded), big current temp + condition icon + description, "H:/L:" today's
range. Middle: 6-hour strip, each column a small icon + temp. Bottom: 5 day
rows, each a weekday label + condition icon + lo/hi temps + range bar.

### 4. Visual improvements

Same `COL_*` palette as every other page — no new colors for weather icons
or the range bars, so night mode's red-luminance remap covers this page for
free like every other.

### 5. Interaction improvements

No in-page controls, same as Clock — exit is the shared footer's Home
button (the 5-day forecast rows were shortened slightly, and their
condition icon shrank from r=9 to r=7, to leave room for the footer below
them). A failed poll (Wi-Fi blip, API timeout) leaves the last successful
reading on screen rather than blanking to "--", the same last-known-good
behavior the dashboard's price already has.

### 6. Reusable components

`drawWeatherIcon()`, `drawWeatherDayBar()` (a forecast-scaled sibling of the
dashboard's single-value range bar), and the "H:/L:" temperature layout —
available for any future forecast-shaped surface.

---

## Navigation & information architecture

- **HOME is the hub.** `HOME → tap a tile → {BTC TICKER, CLOCK, Settings,
  Weather}`, and every app's only way back is the **Home button in the
  shared footer** (`uiDrawFooter()`, right side) — a plain tap, hit-tested
  against `UI_FOOTER_HOME_*` ahead of each page's own tap handling. There is
  no per-page back/close chrome left: the dashboard gear, the status-bar
  clock tap, the clock's "X," and the settings list's `<` are all gone (see
  "HOME (app launcher)" above for the rationale). An earlier iteration
  replaced those with a long-press-anywhere (>3s) gesture instead of a
  button; that was itself replaced once the shared footer existed to hold a
  visible, discoverable control, and the `LONG_PRESS_MS`/`pressStartMs`
  timer machinery in `btc_ticker.ino` was deleted along with it. The settings
  **picker**'s `<` (picker → list) still exists — that's in-app hierarchy,
  not app exit, same as `Picker → Confirm`.
- The shared footer itself is not dashboard-only chrome: `uiDrawFooter()` is
  called at the end of `uiRender()` (dashboard), `uiRenderClock()`,
  `uiRenderWeather()`, `uiRenderHome()`, and `uiRenderSettings()` (the
  top-level list only — the picker and confirmation sub-pages don't draw
  it, and reuse that screen band for their own rows/buttons instead).
- Page state in the .ino: `Page::{HOME,DASHBOARD,SETTINGS,CLOCK,WIFI_SETUP,
  SPLASH,WEATHER}` plus `clockMode` (0/1/2) and the existing `confirmIdx`
  (confirm remains a property of the picker, not a top-level page). HOME is
  the default and the fallback restore target; HOME/DASHBOARD/CLOCK/WEATHER
  are the four restorable pages (NVS `s.page`, see `RESTORABLE_PAGES` in the
  .ino), so the last app you were in comes back after a reset.
- The simulator mirrors the same state machine — including the footer Home
  button's hit box (`UI_FOOTER_HOME_*`) and its own `drawFooter()` — so every
  flow can be exercised without hardware.

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
