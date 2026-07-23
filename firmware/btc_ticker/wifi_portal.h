#pragma once

// Phone-facing Wi-Fi setup: a SoftAP + captive portal ("Find Access Mode")
// entered from the Settings "Forget Wi-Fi network" row. A phone joins the
// open AP, gets redirected to a one-page SSID/password form, and submitting
// it saves the credentials (wifi_creds.h) and reboots into normal operation
// on the new network.

// SSID of the open setup access point.
extern const char* const WIFI_PORTAL_SSID;

// Tears down STA, brings up the SoftAP, and starts the DNS (captive-portal
// redirect) and HTTP servers. Call once when entering Page::WIFI_SETUP.
// Caller is responsible for releasing the price-poll TLS session
// (fetchPriceRelease()) beforehand — that's a net_price.* concern, not this
// module's.
void wifiPortalStart();

// Services the DNS + HTTP servers; call every loop() pass while
// Page::WIFI_SETUP is active. Internally reboots (esp_restart()) a short
// delay after a form submission is saved, once the HTTP response has had
// time to flush.
void wifiPortalLoop();
