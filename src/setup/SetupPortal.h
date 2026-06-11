#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

class Settings;
class Display;

// Captive-portal access point used for first-time WiFi/units configuration.
// Hosts a small form, captures the submission into Settings, then restarts.
class SetupPortal {
 public:
  void begin(Settings& settings, Display& display);
  void handle();  // pump from loop()

 private:
  void handleRoot();
  void handleExit();
  bool captivePortal();
  void sendCommonHead();
  void streamConfigHtml();
  // Scans for nearby networks in STA mode and caches the de-duplicated SSIDs.
  // Run before the access point starts so the AP is stable and the list instant.
  void scanNetworks();
  // Streams the <option> list of the cached nearby networks.
  void streamNetworkOptions();
  // Streams a dynamic field value, skipping empties (an empty chunked
  // sendContent would prematurely terminate the response).
  void sendValue(const String& value);
  String randomHandle();

  static constexpr int MAX_NETWORKS = 20;

  Settings* settings_ = nullptr;
  Display* display_ = nullptr;
  WebServer server_{80};
  DNSServer dnsServer_;
  String exitHandle_;
  String networks_[MAX_NETWORKS];  // cached SSIDs from the pre-AP scan
  int networkCount_ = 0;
};
