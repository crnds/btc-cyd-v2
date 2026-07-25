#pragma once
#include <stdint.h>
#include <stddef.h>

// Runtime Wi-Fi credentials, stored in NVS (Preferences namespace "wifi") so
// they can be replaced from the phone-facing setup portal (wifi_portal.*)
// without a reflash. Deliberately its own namespace, separate from the
// "ticker" settings namespace in settings.*.
//
// config.h's WIFI_SSID/WIFI_PASSWORD remain the compiled fallback: if this
// store is empty (fresh flash, or after wifiCredsClear() with no new
// credentials submitted yet), the caller connects with those instead. See
// wifiBeginConfigured() in btc_ticker.ino.

// Max SSID/password lengths per the 802.11 spec (32 bytes / 63 chars), +1 for
// the terminator.
static const size_t WIFI_CREDS_SSID_LEN = 33;
static const size_t WIFI_CREDS_PASS_LEN = 64;

// Loads stored credentials into the caller's buffers. Returns false (buffers
// left untouched) if none are stored.
bool wifiCredsLoad(char* ssid, size_t ssidLen, char* pass, size_t passLen);

// Persists ssid/pass into NVS via Preferences::putString(), which stores
// the string exactly as given (no truncation here). Callers must enforce
// WIFI_CREDS_SSID_LEN/WIFI_CREDS_PASS_LEN before calling (see
// wifi_portal.cpp's handleSave()); wifiCredsLoad() truncates on read as a
// second line of defense if a caller doesn't.
void wifiCredsSave(const char* ssid, const char* pass);

// Erases stored credentials so the next wifiCredsLoad() call reports none.
void wifiCredsClear();
