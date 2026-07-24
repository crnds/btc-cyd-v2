#include <Arduino.h>
#include "net_klines.h"
#include "net_http.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <stdlib.h>

static const char* KLINES_URL_FMT =
    "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=%s&limit=%d";

// Binance klines come as [[openTime,"open","high","low","close","volume",...],...].
// The full payload can be ~90KB (limit=288); buffering it in a JSON document
// would hold every candle's strings in RAM, so instead extract fields 0-4
// (openTime, o, h, l, c) of each candle straight off the TLS stream with a
// bracket-depth scanner. Field 0 (openTime) is a bare number; fields 1-4 are
// quoted; fields 5+ (volume, closeTime, ...) are skipped entirely.
static int parseKlines(HTTPClient& http, CandleRec* out, int outCap) {
  Stream& s = http.getStream();
  int depth = 0, field = 0, n = 0;
  bool inStr = false;
  char buf[24];
  int bl = 0;
  CandleRec cur;
  uint32_t lastByte = millis();

  while (millis() - lastByte < HTTP_TIMEOUT_MS) {
    int c = s.read();
    if (c < 0) {
      if (!http.connected()) break;
      delay(2);
      continue;
    }
    lastByte = millis();
    char ch = (char)c;

    if (inStr) {
      if (ch == '"') inStr = false;
      else if (depth == 2 && field >= 1 && field <= 4 && bl < (int)sizeof(buf) - 1) buf[bl++] = ch;
      continue;
    }
    switch (ch) {
      case '"':
        inStr = true;
        break;
      case '[':
        depth++;
        if (depth == 2) { field = 0; bl = 0; }
        break;
      case ',':
        if (depth == 2) {
          buf[bl] = '\0';
          switch (field) {
            case 0: cur.openEpoch = (uint32_t)(strtoull(buf, nullptr, 10) / 1000ULL); break;
            case 1: cur.o = atof(buf); break;
            case 2: cur.h = atof(buf); break;
            case 3: cur.l = atof(buf); break;
            case 4: cur.c = atof(buf); break;
            default: break;
          }
          field++;
          bl = 0;
        }
        break;
      case ']':
        if (depth == 2) {
          if (n < outCap) out[n] = cur;
          n++;
        }
        depth--;
        if (depth == 0) return n;
        break;
      default:
        if (depth == 2 && field == 0 && bl < (int)sizeof(buf) - 1) buf[bl++] = ch;
        break;
    }
  }
  return 0;  // timed out mid-body
}

int klinesFetch(const char* interval, int limit, CandleRec* out, int outCap) {
  char url[160];
  snprintf(url, sizeof(url), KLINES_URL_FMT, interval, limit);

  WiFiClientSecure tls;
  HTTPClient http;
  if (!httpGet(http, tls, url)) return 0;

  int n = parseKlines(http, out, outCap);
  http.end();
  return n;
}
