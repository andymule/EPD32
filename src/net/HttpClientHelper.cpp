#include "net/HttpClientHelper.h"

#include "Config.h"
#include "Log.h"

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

void HttpClientHelper::end() {
  if (active_) {
    http_.end();
    active_ = false;
  }
}
