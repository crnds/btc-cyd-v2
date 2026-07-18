#include "net_price.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static const char* PRICE_URL = "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT";
static const uint32_t HTTP_TIMEOUT_MS = 8000;

// File-scope (not function-local) so fetchPriceRelease() can tear the
// session down from outside fetchPrice() — see its declaration in the
// header for why that's needed.
static WiFiClientSecure tls;
static HTTPClient http;
static bool fresh = true;  // true -> (re)configure + full handshake next GET

// Price polls at 1 Hz — a fresh TLS handshake per poll costs ~1-2s of mbedTLS
// CPU. This module keeps ONE keep-alive HTTP/1.1 session to Binance so a
// steady-state poll is a single request on the already-open socket.
bool fetchPrice(float& price, float& changePct, float& high24h, float& low24h) {
  if (fresh) {
    tls.setInsecure();           // v1: no cert pinning
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    http.setReuse(true);         // default, kept explicit: HTTP/1.1 keep-alive
    fresh = false;
    // NOTE: never call useHTTP10() here — it forces _reuse=false in
    // HTTPClient, and an HTTP/1.0 response can't keep the socket alive.
  }

  bool ok = http.begin(tls, PRICE_URL);
  int code = ok ? http.GET() : -1;
  if (code != HTTP_CODE_OK) {
    Serial.printf("price GET failed: code %d\n", code);
    // full teardown so the next attempt starts with a clean TCP+TLS
    // handshake — covers server-closed-idle-socket / half-open cases
    http.end();
    tls.stop();
    fresh = true;
    return false;
  }

  String body = http.getString();
  http.end();                    // keeps the socket open when reusable

  JsonDocument filter;
  filter["lastPrice"] = true;
  filter["priceChangePercent"] = true;
  filter["highPrice"] = true;
  filter["lowPrice"] = true;
  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) return false;

  const char* p = doc["lastPrice"];
  const char* c = doc["priceChangePercent"];
  const char* h = doc["highPrice"];
  const char* l = doc["lowPrice"];
  if (!p) return false;
  price = atof(p);
  changePct = c ? atof(c) : NAN;
  high24h = h ? atof(h) : NAN;
  low24h = l ? atof(l) : NAN;
  return price > 0;
}

// Tears down the persistent session so its ~45KB of mbedTLS session heap is
// released before a heavier one-shot fetch (klines backfill) needs headroom
// for its own handshake. The next fetchPrice() call transparently re-connects.
void fetchPriceRelease() {
  if (fresh) return;
  http.end();
  tls.stop();
  fresh = true;
}
