#include <Arduino.h>
#include "store.h"
#include <LittleFS.h>
#include <math.h>

static const char* CANDLES_PATH = "/candles.bin";
static const size_t REC_SIZE = sizeof(CandleRec);  // 20 bytes (packed)

struct __attribute__((packed)) StoreHeader {  // 8 bytes
  uint32_t magic;
  uint32_t intervalSec;
};
static const uint32_t STORE_MAGIC = 0x31444E43;  // 'CND1' little-endian
static const size_t HEADER_SIZE = sizeof(StoreHeader);
static const size_t FILE_SIZE = HEADER_SIZE + REC_SIZE * MAX_CANDLES;

// Sane BTC price ceiling for range-checking — generous enough to never be
// the thing that rejects a real candle, just enough to catch torn writes.
static const float MAX_SANE_PRICE = 10000000.0f;

static bool validRecord(const CandleRec& r, time_t now, uint32_t intervalSec) {
  if (r.openEpoch == 0) return false;
  if (r.openEpoch % intervalSec != 0) return false;
  if (now >= NTP_VALID_EPOCH) {
    if ((int64_t)r.openEpoch < (int64_t)now - (int64_t)intervalSec * MAX_CANDLES) return false;
    if ((int64_t)r.openEpoch > (int64_t)now + (int64_t)intervalSec) return false;
  }
  if (r.o <= 0 || r.h <= 0 || r.l <= 0 || r.c <= 0) return false;
  if (r.o > MAX_SANE_PRICE || r.h > MAX_SANE_PRICE) return false;
  float lo = fminf(r.o, r.c);
  float hi = fmaxf(r.o, r.c);
  if (r.l > lo + 0.01f) return false;
  if (r.h < hi - 0.01f) return false;
  return true;
}

static void recreateFile(uint32_t intervalSec) {
  File f = LittleFS.open(CANDLES_PATH, "w");
  if (!f) {
    Serial.println("store: failed to create candles.bin");
    return;
  }
  StoreHeader hdr = {STORE_MAGIC, intervalSec};
  f.write((const uint8_t*)&hdr, HEADER_SIZE);
  CandleRec zero = {0, 0, 0, 0, 0};
  for (int i = 0; i < MAX_CANDLES; i++) f.write((const uint8_t*)&zero, REC_SIZE);
  f.close();
}

void storeReset(uint32_t intervalSec) {
  recreateFile(intervalSec);
}

int storeInit(uint32_t intervalSec) {
  if (!LittleFS.begin(true)) {  // formatOnFail
    Serial.println("store: LittleFS mount failed even after format");
    return 0;
  }

  File f = LittleFS.open(CANDLES_PATH, "r");
  StoreHeader hdr = {0, 0};
  if (f) {
    if (f.size() == FILE_SIZE) f.read((uint8_t*)&hdr, HEADER_SIZE);
  }
  if (!f || f.size() != FILE_SIZE || hdr.magic != STORE_MAGIC || hdr.intervalSec != intervalSec) {
    if (f) f.close();
    Serial.println("store: candles.bin missing/wrong size/interval, recreating");
    recreateFile(intervalSec);
    return 0;
  }

  // NTP hasn't run yet at this point in boot — validRecord skips the
  // recency bound when `now` predates NTP_VALID_EPOCH and relies on the
  // structural/range checks alone; backfill repairs the rest within seconds.
  time_t now = time(nullptr);
  int valid = 0;
  for (int i = 0; i < MAX_CANDLES; i++) {
    CandleRec rec;
    if (f.read((uint8_t*)&rec, REC_SIZE) != (int)REC_SIZE) break;
    if (validRecord(rec, now, intervalSec)) {
      candleRing[i] = rec;
      valid++;
    } else {
      candleRing[i] = {0, 0, 0, 0, 0};
    }
  }
  f.close();
  Serial.printf("store: loaded %d/%d valid candles\n", valid, MAX_CANDLES);
  return valid;
}

void storeWriteCandle(int slot, const CandleRec& rec) {
  File f = LittleFS.open(CANDLES_PATH, "r+");
  if (!f) {
    // Shouldn't happen post-init, but recreate and retry once rather than
    // silently dropping writes for the rest of the session.
    recreateFile(candleSeconds);
    f = LittleFS.open(CANDLES_PATH, "r+");
    if (!f) return;
  }
  f.seek(HEADER_SIZE + (uint32_t)slot * REC_SIZE, SeekSet);
  f.write((const uint8_t*)&rec, REC_SIZE);
  f.close();
}
