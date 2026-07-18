#include <Arduino.h>
#include "candles.h"
#include "store.h"
#include <time.h>
#include <math.h>
#include <string.h>

uint32_t candleSeconds = 300;
CandleRec candleRing[MAX_CANDLES];
CandleRec formingCandle = {0, 0, 0, 0, 0};

static uint32_t lastClosedBucket = 0;
static uint32_t lastFormingPersistMs = 0;

static void closeForming() {
if (formingCandle.openEpoch == 0) return;
int slot = candleSlot(formingCandle.openEpoch);
candleRing[slot] = formingCandle;
storeWriteCandle(slot, formingCandle);
uint32_t bucket = formingCandle.openEpoch / candleSeconds;
if (bucket > lastClosedBucket) lastClosedBucket = bucket;
}

void candlesReset(uint32_t newSeconds) {
candleSeconds = newSeconds;
memset(candleRing, 0, sizeof(candleRing));
formingCandle = {0, 0, 0, 0, 0};
lastClosedBucket = 0;
lastFormingPersistMs = 0;
storeReset(newSeconds);
}

void candlesTick(float price) {
if (isnan(price)) return;
time_t now = time(nullptr);
if (now < NTP_VALID_EPOCH) return;

uint32_t bucket = (uint32_t)now / candleSeconds;
uint32_t bucketOpen = bucket * candleSeconds;

if (formingCandle.openEpoch == 0) {
formingCandle = {bucketOpen, price, price, price, price};
} else if (bucketOpen > formingCandle.openEpoch) {
closeForming();
formingCandle = {bucketOpen, price, price, price, price};
} else {
if (price > formingCandle.h) formingCandle.h = price;
if (price < formingCandle.l) formingCandle.l = price;
formingCandle.c = price;
}

// Persist the still-forming candle every 60s so a power-cycle mid-candle
// loses at most a minute of it (it also gets fully overwritten on close).
uint32_t nowMs = millis();
if (nowMs - lastFormingPersistMs >= 60000) {
lastFormingPersistMs = nowMs;
storeWriteCandle(candleSlot(formingCandle.openEpoch), formingCandle);
}
}

void candlesSetClosed(const CandleRec& rec) {
int slot = candleSlot(rec.openEpoch);
candleRing[slot] = rec;
storeWriteCandle(slot, rec);
uint32_t bucket = rec.openEpoch / candleSeconds;
if (bucket > lastClosedBucket) lastClosedBucket = bucket;
}

void candlesSeedForming(const CandleRec& rec) {
formingCandle = rec;
}

bool candlesNeedGapRepair(int& gapCount) {
gapCount = 0;
time_t now = time(nullptr);
if (now < NTP_VALID_EPOCH || lastClosedBucket == 0) return false;
uint32_t curBucket = (uint32_t)now / candleSeconds;
int32_t gap = (int32_t)curBucket - (int32_t)lastClosedBucket;
if (gap > 1) {
gapCount = gap;
return true;
}
return false;
}

