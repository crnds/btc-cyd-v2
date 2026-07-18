#pragma once

// Binance 24h ticker: price (USDT) + 24h % change. changePct is left
// untouched (NAN) if the field is missing.
bool fetchPrice(float& price, float& changePct);

// Releases the persistent keep-alive TLS session's heap. Call before a
// heavier one-shot fetch (e.g. klines backfill) that needs headroom for its
// own handshake; fetchPrice() reconnects transparently on the next call.
void fetchPriceRelease();
