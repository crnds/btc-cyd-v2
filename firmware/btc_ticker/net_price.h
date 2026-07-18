#pragma once

// Binance 24h ticker: price (USDT), 24h % change, and 24h high/low (for the
// dashboard range-position bar). changePct/high24h/low24h are left untouched
// (NAN) if their field is missing.
bool fetchPrice(float& price, float& changePct, float& high24h, float& low24h);

// Releases the persistent keep-alive TLS session's heap. Call before a
// heavier one-shot fetch (e.g. klines backfill) that needs headroom for its
// own handshake; fetchPrice() reconnects transparently on the next call.
void fetchPriceRelease();
