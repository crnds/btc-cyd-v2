# CLAUDE.md

## UI conventions (firmware/btc_ticker)

- **20px right-edge padding.** Nothing should render past `CONTENT_RIGHT` (`SCREEN_W - PAD_RIGHT`, currently 300 of 320px) — this applies to both the dashboard and the Settings page. Right-aligned text, separator lines, and borders must stop at `CONTENT_RIGHT`, not `SCREEN_W`. See `ui.cpp` (`drawStatusBar`, `drawChart`, `uiRenderSettings`).
