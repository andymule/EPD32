#pragma once
void StreamConfigHTML();

const char* selfhostedWifiName = "Atmo";
String randomExitHandle = "";
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
const char *_atmoSetupURL = "at.mo";

boolean isIp(String str) {
	for (unsigned int i = 0; i < str.length(); i++) {
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9')) {
			return false;
		}
	}
	return true;
}

String toStringIp(IPAddress ip) {
	String res = "";
	for (int i = 0; i < 3; i++) {
		res += String((ip >> (8 * i)) & 0xFF) + ".";
	}
	res += String(((ip >> 8 * 3)) & 0xFF);
	return res;
}

boolean captivePortal() {
	if (!isIp(server.hostHeader()) && server.hostHeader() != (String(_atmoSetupURL) + ".local")) {
		pp("Request redirected to captive portal");
		server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
		server.send(302, "text/plain", "");
		server.client().stop();
		return true;
	}
	return false;
}

String RandomHandle()
{
	randomSeed(analogRead(0));
	int rand = random(1000, 1000000);
	return String(rand);
}

void handle_ExitSetup()
{
	for (int i = 0; i < server.args(); i++) {
		if (server.argName(i) == "n")
			Prefs.putString(PREF_SSID_STRING, server.arg(i));
		if (server.argName(i) == "p")
			Prefs.putString(PREF_PASSWORD_STRING, server.arg(i));
		if (server.argName(i) == "metric")
			Prefs.putBool(PREF_METRIC_BOOL, server.arg(i) == "true");
	}

	gfx.fillScreen(GxEPD_WHITE);
	gfx.nextPage();
	Prefs.putBool(PREF_VALID_BOOL, false);

	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", "");
	server.sendContent(F("<!DOCTYPE html><html><head>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">"
		"<title>Atmo Weather Friend</title>"
		"<style>html{font-family:Helvetica;display:inline-block;margin:0 auto;text-align:center;}"
		"body{margin-top:50px;}h1{color:#444;margin:50px auto 30px;}h3{color:#444;margin-bottom:50px;}"
		"</style></head><body>"
		"<h1>Settings saved!</h1><br>"
		"<h3>Check your Atmo device to verify the connection.</h3>"
		"</body></html>"));
	server.client().stop();

	Prefs.end();
	gfx.fillScreen(GxEPD_BLACK);
	gfx.nextPage();
	gfx.fillScreen(GxEPD_WHITE);
	gfx.nextPage();
	esp_deep_sleep(1 * OneSecond);
}

void handle_OnConnect()
{
	if (captivePortal()) {
		return;
	}
	pp("New Connection!");
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", "");
	StreamConfigHTML();
	server.client().stop();
}

void HostWebsiteForInit()
{
	IPAddress iip(1, 2, 3, 4);
	IPAddress isubnet(255, 255, 255, 0);
	WiFi.softAP(selfhostedWifiName);
	delay(200);
	WiFi.softAPConfig(iip, iip, isubnet);
	delay(200);

	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, "*", iip);
	server.onNotFound(handle_OnConnect);

	randomExitHandle = RandomHandle();
	server.on("/" + randomExitHandle, handle_ExitSetup);

	server.begin();

	IPAddress IP = WiFi.softAPIP();
	pp("page address: ");
	pp(IP);
}

void StreamConfigHTML() {
	server.sendContent(F("<!DOCTYPE html><html><head>"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">"
		"<title>Atmo Weather Friend</title>"
		"<style>html{font-family:Helvetica;display:inline-block;margin:0 auto;text-align:center;}"
		"body{margin-top:50px;}h1{color:#444;margin:50px auto 30px;}h3{color:#444;margin-bottom:50px;}"
		"p{font-size:14px;color:#888;margin-bottom:10px;}"
		"</style></head><body>"
		"<h1>Atmo</h1><h3>Configuration</h3>"
		"<form action=\"/"));
	server.sendContent(randomExitHandle);
	server.sendContent(F("\" method=GET>WiFi Network: <input type=text name=n value=\""));
	server.sendContent(Prefs.getString(PREF_SSID_STRING));
	server.sendContent(F("\"><br><br>WiFi Password: <input type=text name=p value=\""));
	server.sendContent(Prefs.getString(PREF_PASSWORD_STRING));
	server.sendContent(F("\"><br><br>"));

	if (Prefs.getBool(PREF_METRIC_BOOL)) {
		server.sendContent(F("<input type=radio name=metric checked value=true> Metric<br>"
			"<input type=radio name=metric value=false> Imperial<br>"));
	} else {
		server.sendContent(F("<input type=radio name=metric value=true> Metric<br>"
			"<input type=radio name=metric checked value=false> Imperial<br>"));
	}

	server.sendContent(F("<br><br><input type=submit value=\"Save and Restart Atmo\"></form>"
		"</body></html>"));
}
