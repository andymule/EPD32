#include "setup/SetupPortal.h"

#include <WiFi.h>
#include <esp_random.h>
#include <esp_sleep.h>

#include "Config.h"
#include "Log.h"
#include "display/Display.h"
#include "model/ForecastCache.h"
#include "settings/Settings.h"

namespace {

bool isIp(const String& str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) return false;
  }
  return true;
}

String toStringIp(IPAddress ip) {
  String res;
  for (int i = 0; i < 3; i++) res += String((ip >> (8 * i)) & 0xFF) + ".";
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}

// Escapes a string for safe use inside a double-quoted HTML attribute. SSIDs and
// WiFi passwords may contain ", &, <, or >; without escaping a quote would break
// out of the value attribute and corrupt the pre-filled setup form.
String htmlAttrEscape(const String& in) {
  String out;
  out.reserve(in.length());
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '"': out += F("&quot;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      default: out += c; break;
    }
  }
  return out;
}

}  // namespace

void SetupPortal::begin(Settings& settings, Display& display) {
  settings_ = &settings;
  display_ = &display;

  // Scan for nearby networks first, while still a pure station. Doing this
  // before the AP exists means the scan's channel-hopping can't disrupt the
  // access point the user is joining, and the pick-list is ready on first load.
  scanNetworks();

  // Pure AP mode for the portal: no station activity to glitch the connection.
  WiFi.mode(WIFI_AP);
  IPAddress ip(1, 2, 3, 4);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAP(setup_portal::AP_NAME);
  delay(200);
  WiFi.softAPConfig(ip, ip, subnet);
  delay(200);

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer_.start(setup_portal::DNS_PORT, "*", ip);

  server_.onNotFound([this]() { handleRoot(); });
  exitHandle_ = randomHandle();
  // Credentials are submitted via POST so they never appear in a URL or logs.
  server_.on("/" + exitHandle_, HTTP_POST, [this]() { handleExit(); });
  server_.begin();

  LOGLN("Setup portal address:");
  LOGLN(WiFi.softAPIP());
}

void SetupPortal::scanNetworks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // ensure the radio is idle and ready to scan
  // Synchronous active scan with a bounded 200ms dwell per channel (still longer
  // than a typical 100ms beacon interval, so nearby APs are reliably seen) to
  // keep the AP appearing quickly. No AP is up yet, so nothing is disturbed.
  int found = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false,
                                /*passive=*/false, /*max_ms_per_chan=*/200);
  LOGLN("WiFi scan found networks:");
  LOGLN(found);

  // Cache de-duplicated, non-hidden SSIDs (multiple APs often share a name).
  // Copying them out now means they survive the upcoming switch to AP mode and
  // the scanDelete() that frees the driver's results.
  networkCount_ = 0;
  for (int i = 0; i < found && networkCount_ < MAX_NETWORKS; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    bool dup = false;
    for (int j = 0; j < networkCount_; j++) {
      if (networks_[j] == ssid) {
        dup = true;
        break;
      }
    }
    if (!dup) networks_[networkCount_++] = ssid;
  }
  WiFi.scanDelete();
}

void SetupPortal::handle() {
  dnsServer_.processNextRequest();
  server_.handleClient();
}

String SetupPortal::randomHandle() {
  // Hardware RNG: always available and, unlike analogRead(0), doesn't fight the
  // WiFi radio for ADC2 (which logs an error and yields a useless seed).
  return String(esp_random() % 1000000);
}

bool SetupPortal::captivePortal() {
  String host = server_.hostHeader();
  if (!isIp(host) && host != (String(setup_portal::LOCAL_HOSTNAME) + ".local")) {
    LOGLN("Request redirected to captive portal");
    server_.sendHeader("Location",
                       String("http://") + toStringIp(server_.client().localIP()), true);
    server_.send(302, "text/plain", "");
    server_.client().stop();
    return true;
  }
  return false;
}

void SetupPortal::handleRoot() {
  if (captivePortal()) return;
  LOGLN("New connection");
  server_.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server_.sendHeader("Pragma", "no-cache");
  server_.sendHeader("Expires", "-1");
  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "text/html", "");
  streamConfigHtml();
  server_.client().stop();
}

void SetupPortal::handleExit() {
  if (server_.hasArg("n")) settings_->setSsid(server_.arg("n"));
  if (server_.hasArg("p")) settings_->setPassword(server_.arg("p"));
  if (server_.hasArg("metric")) settings_->setUseMetric(server_.arg("metric") == "true");
  if (server_.hasArg("style")) {
    int s = server_.arg("style").toInt();
    settings_->setLayoutStyle((s == 0) ? 0 : 1);
  }
  if (server_.hasArg("rot")) {
    int r = server_.arg("rot").toInt();
    if (r < 0 || r > 3) r = display_defaults::ROTATION;
    settings_->setRotation((uint8_t)r);
  }
  auto hourArg = [&](const char* key, uint8_t fallback) -> uint8_t {
    if (!server_.hasArg(key)) return fallback;
    int h = server_.arg(key).toInt();
    return (h >= 0 && h <= 23) ? (uint8_t)h : fallback;
  };
  settings_->setFetchHour(hourArg("fetchHour", sched_defaults::FETCH_HOUR));
  settings_->setQuietStart(hourArg("qStart", sched_defaults::QUIET_START));
  settings_->setQuietEnd(hourArg("qEnd", sched_defaults::QUIET_END));
  settings_->setValid(false);
  // Drop any cached forecast so the next boot does a full fetch (picks up a unit,
  // location, or network change immediately instead of at the daily refresh).
  cache::invalidate();

  server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_.send(200, "text/html", "");
  sendCommonHead();
  server_.sendContent(F(
      "<body><div class=card><h1>Saved</h1>"
      "<p class=sub>Atmo is restarting and will connect now. "
      "You can close this page.</p></div></body></html>"));
  server_.client().stop();

  settings_->end();
  display_->showSettingsSaved();
  esp_deep_sleep(1 * timing::ONE_SECOND_US);
}

void SetupPortal::sendCommonHead() {
  server_.sendContent(F(
      "<!DOCTYPE html><html lang=en><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Atmo</title><style>"
      ":root{color-scheme:light dark}*{box-sizing:border-box}"
      "body{margin:0;min-height:100vh;display:flex;align-items:center;"
      "justify-content:center;padding:20px;background:#f2f2f7;color:#1c1c1e;"
      "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,"
      "Arial,sans-serif}"
      ".card{background:#fff;max-width:380px;width:100%;border-radius:16px;"
      "box-shadow:0 10px 30px rgba(0,0,0,.08);padding:28px}"
      "h1{margin:0 0 4px;font-size:28px}"
      ".sub{margin:0 0 20px;color:#8e8e93;font-size:15px}"
      "label{display:block;font-size:13px;color:#3c3c43;margin:14px 0 6px;"
      "font-weight:600}"
      "input[type=text],input[type=password],select{width:100%;padding:12px 14px;"
      "font-size:16px;border:1px solid #d1d1d6;border-radius:10px;outline:none;"
      "background:#fff;color:inherit}"
      "select{margin-bottom:8px}"
      "input:focus,select:focus{border-color:#0a84ff}"
      ".row{display:flex;gap:10px;margin-top:16px}"
      ".row label{flex:1;display:flex;align-items:center;justify-content:center;"
      "gap:8px;margin:0;padding:12px;border:1px solid #d1d1d6;border-radius:10px;"
      "font-weight:500;cursor:pointer}"
      ".show{display:flex;align-items:center;gap:6px;margin-top:8px;font-size:13px;"
      "color:#8e8e93;font-weight:400}"
      "button{width:100%;margin-top:22px;padding:14px;font-size:17px;"
      "font-weight:600;color:#fff;background:#0a84ff;border:0;border-radius:12px;"
      "cursor:pointer}button:active{background:#0060df}"
      "@media(prefers-color-scheme:dark){body{background:#000;color:#fff}"
      ".card{background:#1c1c1e}input[type=text],input[type=password],select"
      "{background:#2c2c2e;color:#fff;border-color:#3a3a3c}"
      ".row label{border-color:#3a3a3c}}"
      "</style></head>"));
}

void SetupPortal::sendValue(const String& value) {
  if (value.length()) server_.sendContent(value);
}

void SetupPortal::streamOption(int value, const char* label, int current) {
  server_.sendContent(F("<option value="));
  server_.sendContent(String(value));
  if (value == current) server_.sendContent(F(" selected"));
  server_.sendContent(F(">"));
  server_.sendContent(label);
  server_.sendContent(F("</option>"));
}

void SetupPortal::streamHourSelect(const char* name, int current) {
  server_.sendContent(F("<select name="));
  server_.sendContent(name);
  server_.sendContent(F(">"));
  for (int h = 0; h < 24; h++) {
    int h12 = h % 12;
    if (h12 == 0) h12 = 12;
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%d %s", h12, h < 12 ? "AM" : "PM");
    streamOption(h, lbl, current);
  }
  server_.sendContent(F("</select>"));
}

void SetupPortal::streamNetworkOptions() {
  for (int i = 0; i < networkCount_; i++) {
    // Each cached SSID is non-empty, so the escaped value is too (no empty-chunk
    // hazard). The browser decodes the entities, so the option's JS value is the
    // real SSID that gets copied into the submitted text field.
    String esc = htmlAttrEscape(networks_[i]);
    server_.sendContent(F("<option value=\""));
    server_.sendContent(esc);
    server_.sendContent(F("\">"));
    server_.sendContent(esc);
    server_.sendContent(F("</option>"));
  }
}

void SetupPortal::streamConfigHtml() {
  sendCommonHead();
  server_.sendContent(F(
      "<body><div class=card><h1>Atmo</h1>"
      "<p class=sub>Connect your weather display</p>"
      "<form method=POST action=\"/"));
  server_.sendContent(exitHandle_);
  server_.sendContent(F(
      "\"><label for=n>WiFi network</label>"
      // Picking from the scanned list copies the SSID into the text field, which
      // remains the submitted source of truth (and supports hidden networks).
      "<select id=picker onchange=\"if(this.value)"
      "document.getElementById('n').value=this.value\">"
      "<option value=\"\">Choose a network\xE2\x80\xA6</option>"));
  streamNetworkOptions();
  server_.sendContent(F(
      "</select>"
      "<input id=n type=text name=n autocapitalize=off autocorrect=off "
      "placeholder=\"or enter a hidden network\" value=\""));
  // Guard: with chunked transfer encoding, sendContent("") emits the zero-length
  // terminating chunk and ends the whole response. On a fresh device the stored
  // SSID/password are empty, so streaming them directly would truncate the form
  // right here. Only send the prefill when there's actually a value.
  sendValue(htmlAttrEscape(settings_->ssid()));
  server_.sendContent(F(
      "\"><label for=p>Password</label>"
      "<input id=p type=password name=p value=\""));
  sendValue(htmlAttrEscape(settings_->password()));
  server_.sendContent(F(
      "\"><label class=show><input type=checkbox onchange=\""
      "document.getElementById('p').type=this.checked?'text':'password'\"> "
      "Show password</label><div class=row>"));

  if (settings_->useMetric()) {
    server_.sendContent(
        F("<label><input type=radio name=metric value=false> &deg;F</label>"
          "<label><input type=radio name=metric value=true checked> &deg;C</label>"));
  } else {
    server_.sendContent(
        F("<label><input type=radio name=metric value=false checked> &deg;F</label>"
          "<label><input type=radio name=metric value=true> &deg;C</label>"));
  }

  server_.sendContent(F("</div>"));  // close the units row

  int style = settings_->layoutStyle();
  server_.sendContent(F("<label for=style>Layout</label>"
                        "<select id=style name=style>"));
  streamOption(1, "Horizon", style);
  streamOption(0, "Dashboard", style);
  server_.sendContent(F("</select>"));

  int rot = settings_->rotation();
  server_.sendContent(F("<label for=rot>Orientation</label>"
                        "<select id=rot name=rot>"));
  streamOption(0, "Portrait", rot);
  streamOption(2, "Portrait (flipped)", rot);
  streamOption(1, "Landscape", rot);
  streamOption(3, "Landscape (flipped)", rot);
  server_.sendContent(F("</select>"));

  server_.sendContent(F("<label>Daily update time</label>"));
  streamHourSelect("fetchHour", settings_->fetchHour());

  server_.sendContent(F("<label>Quiet hours (pause hourly updates)</label>"
                        "<div class=row>"));
  streamHourSelect("qStart", settings_->quietStart());
  streamHourSelect("qEnd", settings_->quietEnd());
  server_.sendContent(F("</div>"));

  server_.sendContent(
      F("<button type=submit>Save &amp; restart</button>"
        "</form></div></body></html>"));
}
