#pragma once

// Copy this file to config.h and fill in your WiFi details. config.h is gitignored.

#define WIFI_SSID     "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// Weather app: Open-Meteo (https://open-meteo.com) — keyless public API,
// no signup. Location is fixed at build time (no on-device picker).
#define WEATHER_LAT  13.7563
#define WEATHER_LON  100.5018
#define WEATHER_CITY "Bangkok"
