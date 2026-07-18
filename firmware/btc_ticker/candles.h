#pragma once
#include <stdint.h>
#include <time.h>

// Compile-time size of the ring/store — the largest visible candle count any
// settings combo can produce (288 5m candles = 24h). Runtime range/interval
// selection always resolves to a visible count <= MAX_CANDLES.
static const int MAX_CANDLES = 288;

// Candle bucket width in seconds; runtime-configurable via Settings (default
// 5m). Changing it invalidates the ring's slot assignments, so callers must
// go through candlesReset() rather than assigning this directly.
extern uint32_t candleSeconds;

// Anything before this UTC epoch means NTP hasn't synced yet (~2023-11-14);
// guards against trusting candles built off the ESP32's un-synced clock.
static const time_t NTP_VALID_EPOCH = 1700000000;

struct __attribute__((packed)) CandleRec {  // 20 bytes
uint32_t openEpoch;  // UTC sec, multiple of candleSeconds; 0 = empty/gap
float o, h, l, c;
};

extern CandleRec candleRing[MAX_CANDLES];
extern CandleRec formingCandle;  // in-progress candle, openEpoch==0 if none yet

inline int candleSlot(uint32_t openEpoch) {
return (int)((openEpoch / candleSeconds) % MAX_CANDLES);
}

// Switches the candle bucket width, wiping the RAM ring, the forming candle,
// and the flash store (old records are keyed to the previous interval's slot
// math and can't be reinterpreted). Caller is responsible for triggering a
// fresh backfill afterward.
void candlesReset(uint32_t newSeconds);

// Call every loop pass with the latest known price (NAN if none has ever
// been received). Assembles the forming candle and closes it into the ring
// + flash at each 5-minute boundary. Runs regardless of price freshness so
// a candle still closes (flat, at the last known price) while WiFi is down.
void candlesTick(float price);

// Backfill/gap-repair: install an authoritative closed candle directly into
// ring + flash, keyed by its own openEpoch (slot is a pure function of
// time, so out-of-order calls are harmless).
void candlesSetClosed(const CandleRec& rec);

// Backfill: seed the in-progress candle (the last, still-forming element of
// a klines response) without touching ring/flash.
void candlesSeedForming(const CandleRec& rec);

// True if the newest closed candle is more than one bucket behind "now"
// (missed a close while offline, or nothing has closed since boot's
// backfill). gapCount receives the number of missed buckets.
bool candlesNeedGapRepair(int& gapCount);

