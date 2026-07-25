#include "wifi_portal.h"
#include "wifi_creds.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_system.h>

// Open AP — no password, so a phone can join with a single tap. Same
// tradeoff class as tls.setInsecure() in net_http.cpp/net_price.cpp: a
// deliberate v1 simplification for a device that's only ever reachable on
// the owner's own premises, and only briefly, only when they've just chosen
// to put it in setup mode.
const char* const WIFI_PORTAL_SSID = "BTC-Ticker-Setup";

static const IPAddress PORTAL_IP(192, 168, 4, 1);
static const uint16_t DNS_PORT = 53;

static DNSServer dnsServer;
static WebServer server(80);

static bool pendingRestart = false;
static uint32_t pendingRestartAtMs = 0;

static const char PORTAL_HTML[] =
  "<!DOCTYPE html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
  "<title>BTC Ticker Setup</title><style>"
  "body{font-family:sans-serif;background:#08090d;color:#f1f5f9;padding:24px}"
  "h1{font-size:18px}input{width:100%;box-sizing:border-box;padding:10px;margin:6px 0 16px;"
  "background:#161b27;border:1px solid #333;color:#fff;border-radius:6px}"
  "button{width:100%;padding:12px;background:#F4620E;border:0;border-radius:6px;color:#fff;"
  "font-size:16px}</style></head><body>"
  "<h1>Connect BTC Ticker to Wi-Fi</h1>"
  "<form method='POST' action='/save'>"
  "<label>Network name (SSID)</label><input name='ssid' maxlength='32' required>"
  "<label>Password</label><input name='pass' type='password' maxlength='63'>"
  "<button type='submit'>Save &amp; Reboot</button>"
  "</form></body></html>";

static const char SAVED_HTML[] =
  "<!DOCTYPE html><html><body style='font-family:sans-serif;background:#08090d;color:#f1f5f9;"
  "padding:24px'><h1>Saved.</h1><p>The board is rebooting and will connect to your network.</p>"
  "</body></html>";

static void handleRoot() {
  server.send(200, "text/html", PORTAL_HTML);
}

static void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  if (ssid.length() == 0) {
    server.send(400, "text/plain", "SSID required");
    return;
  }
  // Clamp at save time so a non-standard client that ignores the form's
  // maxlength can't silently get truncated later at wifiCredsLoad() time
  // instead, which would leave WiFi.begin() retrying a mangled password.
  if (ssid.length() > WIFI_CREDS_SSID_LEN - 1) ssid = ssid.substring(0, WIFI_CREDS_SSID_LEN - 1);
  if (pass.length() > WIFI_CREDS_PASS_LEN - 1) pass = pass.substring(0, WIFI_CREDS_PASS_LEN - 1);
  wifiCredsSave(ssid.c_str(), pass.c_str());
  server.send(200, "text/html", SAVED_HTML);
  // Give the response time to flush over TCP before tearing everything down.
  pendingRestart = true;
  pendingRestartAtMs = millis();
}

// Every unmatched path (the various OS captive-portal probe URLs — Apple's
// /hotspot-detect.html, Android's /generate_204, etc.) redirects to the
// setup form. That's enough to make phones auto-open the portal without
// hardcoding every vendor's probe path individually.
static void handleNotFound() {
  server.sendHeader("Location", String("http://") + PORTAL_IP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void wifiPortalStart() {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(PORTAL_IP, PORTAL_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(WIFI_PORTAL_SSID);

  dnsServer.start(DNS_PORT, "*", PORTAL_IP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();

  pendingRestart = false;
}

void wifiPortalLoop() {
  dnsServer.processNextRequest();
  server.handleClient();
  if (pendingRestart && millis() - pendingRestartAtMs > 1200) {
    esp_restart();
  }
}
