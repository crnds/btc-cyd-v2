#pragma once
#include <stdint.h>

// One data point of the 6-entry short-range hourly forecast.
struct WxHour {
  uint32_t dt;    // unix epoch (UTC)
  float temp;     // deg C
  uint16_t cond;  // condition id in the taxonomy drawWeatherIcon() (ui.cpp)
                  // switches on — see net_weather.cpp's wmoToCond(), which
                  // translates Open-Meteo's WMO weather codes into it
};

// One data point of the 5-entry daily forecast.
struct WxDay {
  uint32_t dt;  // unix epoch (UTC), noon of that day
  float hi, lo; // deg C
  uint16_t cond;
};

static const int WEATHER_NUM_HOURS = 6;
static const int WEATHER_NUM_DAYS = 5;

struct WeatherData {
  bool valid;             // false until a fetch has fully succeeded
  int32_t tzOffset;       // seconds east of UTC, at the fixed lat/lon (not the device's own TZ)
  float curTemp;          // deg C
  float todayHi, todayLo; // deg C, from the first daily entry
  uint16_t curCond;
  char curDesc[32];       // e.g. "broken clouds"
  WxHour hours[WEATHER_NUM_HOURS];
  WxDay days[WEATHER_NUM_DAYS];
};

// Fetches https://api.open-meteo.com/v1/forecast for the fixed
// WEATHER_LAT/WEATHER_LON (config.h) — a keyless public API (no signup, no
// appid), same philosophy as Binance's price/klines feeds. One-shot fetch
// (its own WiFiClientSecure/HTTPClient, like net_klines.cpp) — caller must
// release the price poll's persistent TLS session first (see
// fetchPriceRelease() in net_price.h) so only one mbedTLS session is ever
// open at a time. Parses straight off the TLS stream with an ArduinoJson
// filter (current temp/weather code, first WEATHER_NUM_HOURS of hourly,
// first WEATHER_NUM_DAYS of daily) so the payload never needs buffering into
// a String. Returns false (out.valid left false) on any HTTP/parse failure
// or missing current-temp field.
bool weatherFetch(WeatherData& out);
