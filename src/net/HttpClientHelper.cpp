#include "net/HttpClientHelper.h"

#include <stdio.h>
#include <string.h>

#include "Config.h"
#include "Log.h"

namespace {
// Parses an RFC 1123 HTTP date ("Wed, 11 Jun 2026 17:53:42 GMT") to a UTC epoch.
time_t parseHttpDate(const String& s) {
  int day, year, hh, mm, ss;
  char mon[4] = {0};
  const char* sp = strchr(s.c_str(), ' ');  // skip the weekday name
  if (!sp) return 0;
  if (sscanf(sp + 1, "%d %3s %d %d:%d:%d", &day, mon, &year, &hh, &mm, &ss) != 6)
    return 0;
  static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* mp = strstr(months, mon);
  if (!mp) return 0;
  int month = static_cast<int>((mp - months) / 3) + 1;  // 1..12
  // Days since 1970-01-01 (Howard Hinnant's civil-from-days, in reverse).
  int y = year - (month <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;
  long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = era * 146097 + doe - 719468;
  return static_cast<time_t>(days) * 86400 + hh * 3600 + mm * 60 + ss;
}
}  // namespace

HttpClientHelper::~HttpClientHelper() { end(); }

int HttpClientHelper::get(const String& url) {
  end();
  const bool https = url.startsWith("https");
  int code = -1;

  for (int attempt = 0; attempt < timing::HTTP_MAX_RETRIES; attempt++) {
    if (https) {
      secureClient_.setInsecure();  // public, non-sensitive data; skip cert chain
      http_.begin(secureClient_, url);
    } else {
      http_.begin(plainClient_, url);
    }
    http_.setTimeout(timing::SITE_TIMEOUT_MS);
    http_.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    static const char* kHeaderKeys[] = {"Date"};  // for an accurate wall clock
    http_.collectHeaders(kHeaderKeys, 1);
    // Refuse compression: body()/getString() de-chunks the response but does not
    // gunzip it, so the body must arrive uncompressed. Do not remove this.
    http_.addHeader("Accept-Encoding", "identity");

    code = http_.GET();
    if (code == 200) {
      active_ = true;
      return code;
    }
    http_.end();
    LOGLN("HTTP attempt " + String(attempt + 1) + " failed: " + String(code));
    // Skip the backoff delay after the last attempt: it would only keep the
    // radio powered with nothing left to try.
    if (attempt + 1 < timing::HTTP_MAX_RETRIES) delay(timing::HTTP_RETRY_DELAY_MS);
  }
  return code;
}

String HttpClientHelper::body() { return http_.getString(); }

time_t HttpClientHelper::serverEpoch() {
  return active_ ? parseHttpDate(http_.header("Date")) : 0;
}

void HttpClientHelper::end() {
  if (active_) {
    http_.end();
    active_ = false;
  }
}
