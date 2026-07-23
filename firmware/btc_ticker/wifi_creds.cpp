#include "wifi_creds.h"
#include <Preferences.h>
#include <string.h>

static const char* NVS_NAMESPACE = "wifi";

bool wifiCredsLoad(char* ssid, size_t ssidLen, char* pass, size_t passLen) {
  Preferences p;
  // begin(readOnly=true) fails (logged as an NVS error) if this namespace
  // was never written — true on every boot until wifiCredsSave() is first
  // called. Bail out on that before touching isKey()/getString() so a
  // never-provisioned device doesn't spam that error every reconnect.
  if (!p.begin(NVS_NAMESPACE, true)) return false;
  bool present = p.isKey("ssid");
  if (present) {
    p.getString("ssid", ssid, ssidLen);
    p.getString("pass", pass, passLen);
  }
  p.end();
  return present;
}

void wifiCredsSave(const char* ssid, const char* pass) {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.end();
}

void wifiCredsClear() {
  Preferences p;
  p.begin(NVS_NAMESPACE, false);
  p.clear();
  p.end();
}
