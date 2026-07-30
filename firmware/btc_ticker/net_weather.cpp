#include <Arduino.h>
#include "net_weather.h"
#include "net_http.h"
#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// forecast_days=5 sizes the daily array to exactly WEATHER_NUM_DAYS.
// forecast_hours=%d makes Open-Meteo start the hourly window at the current
// hour rather than at local midnight — without it, hourly always begins at
// 00:00 of the request day, so the "next N hours" strip would show
// already-elapsed hours for most of the day. We ask for one extra hour
// (WEATHER_NUM_HOURS + 1) and then skip that leading current-hour entry when
// parsing below, so the strip always shows the *next* WEATHER_NUM_HOURS
// hours (e.g. at 14:33 that's 15,16,...,20) rather than starting with the
// mostly-elapsed current hour. timeformat=unixtime + the utc_offset_seconds
// field (read below) give plain epoch math instead of ISO8601 string parsing.
static const char* WEATHER_URL_FMT =
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weather_code&hourly=temperature_2m,weather_code"
    "&daily=temperature_2m_max,temperature_2m_min,weather_code"
    "&timezone=auto&timeformat=unixtime&forecast_days=5&forecast_hours=%d";

// Open-Meteo reports WMO weather codes (https://open-meteo.com/en/docs —
// "WMO Weather interpretation codes"), not OWM's condition ids. Translate
// into the ranges drawWeatherIcon() (ui.cpp) already switches on, so the
// drawing code stays provider-agnostic.
static uint16_t wmoToCond(int code) {
  switch (code) {
    case 0: return 800;                      // clear sky
    case 1: return 801;                      // mainly clear
    case 2: return 802;                      // partly cloudy
    case 3: return 804;                      // overcast
    case 45: case 48: return 741;            // fog
    case 51: case 53: case 55:               // drizzle
    case 56: case 57: return 300;            // freezing drizzle
    case 61: case 63: case 65:               // rain
    case 66: case 67:                        // freezing rain
    case 80: case 81: case 82: return 500;   // rain showers
    case 71: case 73: case 75: case 77:      // snow / snow grains
    case 85: case 86: return 600;            // snow showers
    case 95: case 96: case 99: return 200;   // thunderstorm (+ hail)
    default: return 800;
  }
}

// Open-Meteo has no description string (unlike OWM) — a short label per code.
static const char* wmoDescription(int code) {
  switch (code) {
    case 0: return "clear sky";
    case 1: return "mainly clear";
    case 2: return "partly cloudy";
    case 3: return "overcast";
    case 45: case 48: return "fog";
    case 51: case 53: case 55: return "drizzle";
    case 56: case 57: return "freezing drizzle";
    case 61: case 63: case 65: return "rain";
    case 66: case 67: return "freezing rain";
    case 71: case 73: case 75: case 77: return "snow";
    case 80: case 81: case 82: return "rain showers";
    case 85: case 86: return "snow showers";
    case 95: return "thunderstorm";
    case 96: case 99: return "thunderstorm with hail";
    default: return "unknown";
  }
}

bool weatherFetch(WeatherData& out) {
  out = WeatherData{};

  // The full URL (lat/lon + all the query params below) runs to ~270 chars —
  // a 256-byte buffer silently truncated it here, dropping forecast_hours
  // off the end entirely and falling back to Open-Meteo's "start hourly at
  // local midnight" default (see the comment above WEATHER_URL_FMT), which
  // then stays frozen for the rest of the calendar day. Sized with real
  // margin so a future param addition doesn't reintroduce the same bug
  // silently (snprintf truncates without any error).
  char url[320];
  snprintf(url, sizeof(url), WEATHER_URL_FMT, (double)WEATHER_LAT, (double)WEATHER_LON,
           WEATHER_NUM_HOURS + 1);

  WiFiClientSecure tls;
  HTTPClient http;
  if (!httpGet(http, tls, url)) return false;

  // Unlike OWM's array-of-objects, Open-Meteo returns column arrays
  // (hourly.time[]/temperature_2m[]/weather_code[] all parallel-indexed) —
  // marking a leaf key true keeps that whole scalar array, no [0] wildcard
  // template needed.
  JsonDocument filter;
  filter["utc_offset_seconds"] = true;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["hourly"]["time"] = true;
  filter["hourly"]["temperature_2m"] = true;
  filter["hourly"]["weather_code"] = true;
  filter["daily"]["time"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;
  filter["daily"]["weather_code"] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("weather parse failed: %s\n", err.c_str());
    return false;
  }

  out.tzOffset = doc["utc_offset_seconds"] | 0;
  out.curTemp = doc["current"]["temperature_2m"] | NAN;
  int curCode = doc["current"]["weather_code"] | 0;
  out.curCond = wmoToCond(curCode);
  strncpy(out.curDesc, wmoDescription(curCode), sizeof(out.curDesc) - 1);

  JsonArrayConst dTime = doc["daily"]["time"].as<JsonArrayConst>();
  JsonArrayConst dMax = doc["daily"]["temperature_2m_max"].as<JsonArrayConst>();
  JsonArrayConst dMin = doc["daily"]["temperature_2m_min"].as<JsonArrayConst>();
  JsonArrayConst dCode = doc["daily"]["weather_code"].as<JsonArrayConst>();
  int nDaily = 0;
  for (JsonVariantConst v : dTime) {
    if (nDaily >= WEATHER_NUM_DAYS) break;
    out.days[nDaily].dt = v.as<uint32_t>();
    out.days[nDaily].hi = dMax[nDaily] | NAN;
    out.days[nDaily].lo = dMin[nDaily] | NAN;
    out.days[nDaily].cond = wmoToCond(dCode[nDaily] | 0);
    nDaily++;
  }
  if (nDaily > 0) {
    out.todayHi = out.days[0].hi;
    out.todayLo = out.days[0].lo;
  }

  JsonArrayConst hTime = doc["hourly"]["time"].as<JsonArrayConst>();
  JsonArrayConst hTemp = doc["hourly"]["temperature_2m"].as<JsonArrayConst>();
  JsonArrayConst hCode = doc["hourly"]["weather_code"].as<JsonArrayConst>();
  int nHourly = 0;
  int hIdx = 0;
  for (JsonVariantConst v : hTime) {
    int i = hIdx++;
    if (i == 0) continue;  // skip the leading current-hour entry
    if (nHourly >= WEATHER_NUM_HOURS) break;
    out.hours[nHourly].dt = v.as<uint32_t>();
    out.hours[nHourly].temp = hTemp[i] | NAN;
    out.hours[nHourly].cond = wmoToCond(hCode[i] | 0);
    nHourly++;
  }

  // Require the full expected counts, not just "some" — a truncated
  // response would otherwise leave the unfilled tail of the fixed-size
  // hours[]/days[] arrays zeroed (epoch 0, 0°, cond 800) yet still get
  // rendered as if it were real data.
  out.valid = nDaily == WEATHER_NUM_DAYS && nHourly == WEATHER_NUM_HOURS && !isnan(out.curTemp);
  return out.valid;
}
