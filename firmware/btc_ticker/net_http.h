#pragma once
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// begin + GET; true only on HTTP 200. Caller must http.end() when done with
// the stream. One-shot: opens its own WiFiClientSecure each call — fine at
// the low cadence backfill/gap-repair run at. net_price.cpp is the
// exception: it polls every second and keeps its own persistent keep-alive
// session instead of this path (see net_price.cpp).
bool httpGet(HTTPClient& http, WiFiClientSecure& tls, const char* url,
             const char* headerName = nullptr, const char* headerValue = nullptr);
