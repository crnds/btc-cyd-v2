#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Connect/read timeout shared by every fetch in this codebase (price polls,
// backfill, gap-repair) — also used as net_klines.cpp's stream byte-read
// timeout.
static const uint32_t HTTP_TIMEOUT_MS = 8000;

// Applies the client config shared by every fetch here: no cert pinning (v1
// tradeoff, see AGENTS.md's security notes), the timeout above, and a
// browser-like User-Agent. Callers diverge after this — httpGet() layers on
// HTTP/1.0 + redirect-following for one-shot stream parsing; net_price.cpp's
// fetchPrice() layers on explicit HTTP/1.1 keep-alive reuse instead. Never
// call useHTTP10() on the keep-alive path — it forces _reuse=false.
void configureHttpClient(HTTPClient& http, WiFiClientSecure& tls);

// begin + GET; true only on HTTP 200. Caller must http.end() when done with
// the stream. One-shot: opens its own WiFiClientSecure each call — fine at
// the low cadence backfill/gap-repair run at. net_price.cpp is the
// exception: it polls every second and keeps its own persistent keep-alive
// session instead of this path (see net_price.cpp).
bool httpGet(HTTPClient& http, WiFiClientSecure& tls, const char* url,
             const char* headerName = nullptr, const char* headerValue = nullptr);
