#pragma once
#include "candles.h"

// Fetch /api/v3/klines?symbol=BTCUSDT&interval=<interval>&limit=<limit>,
// streaming the response straight off the TLS socket (no buffering — the
// body can be ~90KB at limit=288). Fills `out` (caller-supplied, >= limit
// entries) with parsed candles in chronological order; out[n-1] is always
// the currently forming (incomplete) candle per Binance's kline semantics.
// Returns the number of candles parsed (0 on failure or a mid-stream timeout).
int klinesFetch(const char* interval, int limit, CandleRec* out, int outCap);
