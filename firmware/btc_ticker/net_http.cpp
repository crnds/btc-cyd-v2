#include "net_http.h"

void configureHttpClient(HTTPClient& http, WiFiClientSecure& tls) {
  tls.setInsecure();                 // v1: no cert pinning
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
}

bool httpGet(HTTPClient& http, WiFiClientSecure& tls, const char* url,
             const char* headerName, const char* headerValue) {
  configureHttpClient(http, tls);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);              // no chunked encoding -> safe to stream-parse
  if (!http.begin(tls, url)) {
    Serial.printf("http.begin failed: %s\n", url);
    return false;
  }
  if (headerName && headerValue) {
    http.addHeader(headerName, headerValue);
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("http.GET failed: code %d for %s\n", code, url);
    http.end();
    return false;
  }
  return true;
}
