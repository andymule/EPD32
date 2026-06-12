#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Thin wrapper over HTTPClient adding retry/backoff and transparent plain-vs-TLS
// client selection based on the URL scheme. Handles one request at a time.
class HttpClientHelper {
 public:
  ~HttpClientHelper();

  // GET with retries; returns the final HTTP status code. On 200, read the
  // response with body() before calling end().
  int get(const String& url);

  // Reads the full response body as a String. This is used (instead of parsing
  // the raw stream) because the weather API replies with chunked transfer
  // encoding; getString() de-chunks it, while a direct stream parse does not.
  String body();

  // UTC epoch parsed from the response's `Date` header (to the second), or 0 if
  // unavailable. Call before end(). Gives an accurate wall clock without an
  // extra NTP round-trip (the weather API's current.time is only 15-min coarse).
  time_t serverEpoch();

  void end();

 private:
  HTTPClient http_;
  WiFiClient plainClient_;
  WiFiClientSecure secureClient_;
  bool active_ = false;
};
