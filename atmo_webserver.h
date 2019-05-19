#pragma once
String SendHTML();

const char* selfhostedWifiName = "ATMO";

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);

String randomExitHandle = "";
String RandomHandle()
{
	randomSeed(analogRead(0));
	int rand = random(1000, 1000000);
	return String(rand);
}

void handle_ExitSetup()
{
	Serial.println("EXIT SETUP!");
	for (int i = 0; i < server.args(); i++) {
		if (server.argName(i) == "n") // wifi name
		{
			savedSettings.wifi_ssid = server.arg(i);
			prefs.putString("wifi_ssid", savedSettings.wifi_ssid);
		}
		if (server.argName(i) == "p") // wifi name
		{
			savedSettings.wifi_password = server.arg(i);
			prefs.putString("wifi_password", savedSettings.wifi_password);
		}
	}
	//Serial.println(server.argName(i) + ":" + server.arg(i));
	//gfx.eraseDisplay();
	//gfx.eraseDisplay(true);
	gfx.fillScreen(GxEPD_WHITE);
	prefs.putBool("valid", true); //invalidate location data // TODO indicate this on display
	String ptr = "<!DOCTYPE html> <html>\n";
	ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
	ptr += "<title>Atmo Weather Friend</title>\n";
	ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
	ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
	ptr += "</style>\n";
	ptr += "</head>\n";
	ptr += "<body>\n";
	ptr += "<h1>Settings saved!!</h1> <br>";
	ptr += "<h3>Check your Atmo device to verify the connection.</h3>\n";
	ptr += "</body>\n";
	ptr += "</html>\n";
	server.send(200, "text/html", ptr);
	//delay(100);	// make sure we write
	server.close();
	delay(100);	// make sure we write
	esp_deep_sleep(1 * OneSecond);	// TODO is this best? prolly need to flag to avoid hard screen blanking
}

void handle_OnConnect() {
	Serial.println("New Connection!");
	//for (int i = 0; i < server.args(); i++) {
	//	if (server.argName(i) == "n") // wifi name
	//	{
	//		savedSettings.wifi_ssid = server.arg(i);
	//	}
	//	if (server.argName(i) == "p") // wifi name
	//	{
	//		savedSettings.wifi_password = server.arg(i);
	//	}
	//	//Serial.println(server.argName(i) + ":" + server.arg(i));
	//}
	server.send(200, "text/html", SendHTML());
}

void HostWebsiteForInit()
{
	WiFi.mode(WIFI_AP);
	WiFi.softAP(selfhostedWifiName);
	delay(50);
	IPAddress iip(8, 8, 8, 8);
	//IPAddress igateway(8, 8, 8, 8);
	IPAddress isubnet(255, 0, 0, 0);
	WiFi.softAPConfig(iip, iip, isubnet);
	dnsServer.start(DNS_PORT, "*", iip);
	/*dnsServer.start(DNS_PORT, "www.at.mo", iip);
	dnsServer.start(DNS_PORT, "at.mo", iip);
	dnsServer.start(DNS_PORT, "at.mo/", iip);
	dnsServer.start(DNS_PORT, "at/mo", iip);
	dnsServer.start(DNS_PORT, "atmo/", iip);*/

	// TODO detect first connection, always route to init connect instead of exit ?
	const String root = "/";
	//auto glambda = [](auto a, auto&& b) { return a < b; };
	//server.onNotFound(webHandleDefault);
	server.on("/", handle_OnConnect);	// this parses an error in intellisense but is totally fine
	//server.on("at/mo", handle_OnConnect);	// this parses an error in intellisense but is totally fine

	randomExitHandle = RandomHandle();
	server.on("/" + randomExitHandle, handle_ExitSetup);	// this parses an error in intellisense but is totally fine

	//server.on(", handle_OnConnect);
	//DrawConnectionInstructions();

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

	ptr += prefs.getString("wifi_ssid");
	ptr += "\"><br><br>";

	ptr += "WiFi Password: <input type=text name=p value=\"";
	ptr += prefs.getString("wifi_password");
	ptr += "\"><br><input type=submit></form>";

	//ptr += "<br><br><a class=\"button button-off\" href=\"/exit\">OFF</a>\n";
	//if (led1stat)
	//{
	//	
	//}
	//else
	//{
	//	ptr += "<p>LED1 Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";
	//}

	//if (led2stat)
	//{
	//	ptr += "<p>LED2 Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";
	//}
	//else
	//{
	//	ptr += "<p>LED2 Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";
	//}

	ptr += "</body>\n";
	ptr += "</html>\n";
	return ptr;
}

void loop() {
	//pp("doin it!");
	dnsServer.processNextRequest();
	server.handleClient();
	//TODO if press pad again, go back to normal operation
}
