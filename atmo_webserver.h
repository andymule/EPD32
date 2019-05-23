#pragma once
// TODO auto captive portal on phone connect
String SendHTML();

const char* selfhostedWifiName = "Atmo";
String randomExitHandle = "";
const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
const char *_atmoSetupURL = "at.mo";

boolean isIp(String str) {
	for (int i = 0; i < str.length(); i++) {
		int c = str.charAt(i);
		if (c != '.' && (c < '0' || c > '9')) {
			return false;
		}
	}
	return true;
}

/** IP to String? */
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
		Serial.println("Request redirected to captive portal");
		server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
		server.send(302, "text/plain", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
		server.client().stop(); // Stop is needed because we sent no content length
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

//void handle_ExitSetup()
void handle_ExitSetup()
{
	//if (captivePortal()) { // If captive portal redirect instead of displaying start page initially
	//	return;
	//}
	//Serial.println("EXIT SETUP!");
	for (int i = 0; i < server.args(); i++) {
		if (server.argName(i) == "n") // wifi name
		{
			prefs.putString(PREF_SSID_STRING, server.arg(i));
		}
		if (server.argName(i) == "p") // wifi pass
		{
			prefs.putString(PREF_PASSWORD_STRING, server.arg(i));
		}
		if (server.argName(i) == "metric") 
		{
			if (server.arg(i) == "true")
			{
				prefs.putBool(PREF_METRIC_BOOL, true);
			}
			else
			{
				prefs.putBool(PREF_METRIC_BOOL, false);
			}
		}
	}

	gfx.fillScreen(GxEPD_WHITE);
	gfx.nextPage();
	prefs.putBool(PREF_VALID_BOOL, true);
	String ptr = "<!DOCTYPE html> <html>\n";
	ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
	ptr += "<title>Atmo Weather Friend</title>\n";
	ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
	ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
	ptr += "</style>\n";
	ptr += "</head>\n";
	ptr += "<body>\n";
	ptr += "<br><br><br><br><br><br><h1>Settings saved!!</h1> <br>";
	ptr += "<h3>Check your Atmo device to verify the connection.</h3>\n";
	ptr += "</body>\n";
	ptr += "</html>\n";
	server.send(200, "text/html", ptr);
	server.close();
	gfx.fillScreen(GxEPD_BLACK);
	gfx.nextPage(); 
	gfx.fillScreen(GxEPD_WHITE);
	gfx.nextPage();
	esp_deep_sleep(1 * OneSecond);	// TODO is this best? prolly need to flag to avoid hard screen blanking?
}

void handle_OnConnect()
{
	if (captivePortal()) { // If captive portal redirect instead of displaying start page initially
		return;
	}
	Serial.println("New Connection!");
	server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	server.sendHeader("Pragma", "no-cache");
	server.sendHeader("Expires", "-1");
	server.setContentLength(CONTENT_LENGTH_UNKNOWN);
	server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
	server.sendContent(SendHTML());
	server.client().stop(); // Stop is needed because we sent no content length
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

	// all four of these might not be needed? does not found cover them all? IDK 
	//server.on("/", handle_OnConnect);
	//server.on("/generate_204", handle_OnConnect);
	//server.on("/fwlink", handle_OnConnect);
	//server.on("/redirect", handle_OnConnect);

	randomExitHandle = RandomHandle();	// random number form so ppl dont accidently reload settings by using saved web address in browser
	server.on("/" + randomExitHandle, handle_ExitSetup);	// this parses an error in intellisense but is totally fine

	server.begin();

	IPAddress IP = WiFi.softAPIP();
	Serial.print("page address: ");
	Serial.println(IP);
}

// TODO drop down auto-populate menu, option for text?
// TODO validate settings on exit?
String SendHTML() {
	String ptr = "<!DOCTYPE html> <html>\n";
	ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
	ptr += "<title>Atmo Weather Friend</title>\n";
	ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
	ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
	ptr += ".button {display: block;width: 80px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
	ptr += ".button-on {background-color: #3498db;}\n";
	ptr += ".button-on:active {background-color: #2980b9;}\n";
	ptr += ".button-off {background-color: #34495e;}\n";
	ptr += ".button-off:active {background-color: #2c3e50;}\n";
	ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
	ptr += "</style>\n";
	ptr += "</head>\n";
	ptr += "<body>\n";
	ptr += "<h1>Atmo</h1>\n";
	ptr += "<h3>Configuration</h3>\n";

	ptr += "<form action=\"/";
	ptr += randomExitHandle;
	ptr += "\" method=GET>WiFi Network: <input type=text name=n value=\"";

	ptr += prefs.getString(PREF_SSID_STRING);
	ptr += "\"><br><br>";

	ptr += "WiFi Password: <input type=text name=p value=\"";
	ptr += prefs.getString(PREF_PASSWORD_STRING);
	ptr += "\"><br><br>";

	ptr += "<input type=radio name=metric";
	if (prefs.getBool(PREF_METRIC_BOOL))
		ptr += " checked ";
	ptr += " value = true > Metric<br>";

	ptr += "<input type=radio name=metric";
	if (!prefs.getBool(PREF_METRIC_BOOL))
		ptr += " checked ";
	ptr += " value = false > Imperial<br>";

	ptr += "<br><br><input type=submit value=\"Save and Restart Atmo\"></form>";

	ptr += "</body>\n";
	ptr += "</html>\n";
	return ptr;
}

void loop() {
	dnsServer.processNextRequest();
	server.handleClient();
}
