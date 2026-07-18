#pragma once
#include "candles.h"

// Mount LittleFS (formatOnFail) and load /candles.bin into candleRing[288].
// The file carries an 8-byte header {magic, intervalSec}; if it's missing,
// the wrong size, has a bad magic, or its intervalSec doesn't match
// `intervalSec`, the file is recreated zero-filled (old data is discarded —
// the ring's slot assignments are meaningless under a different interval).
// Each record is validated on load (epoch%intervalSec==0, within the last
// intervalSec*MAX_CANDLES seconds, l<=min(o,c), h>=max(o,c), sane price
// range); an invalid record is left as a gap (openEpoch=0) rather than
// trusted. Returns the number of valid records loaded.
int storeInit(uint32_t intervalSec);

// Recreates a zero-filled file stamped with the new interval. Called by
// candlesReset() whenever the candle bucket width changes.
void storeReset(uint32_t intervalSec);

// Write a single 20-byte record to its slot (seek(header + slot*20) + write).
void storeWriteCandle(int slot, const CandleRec& rec);
