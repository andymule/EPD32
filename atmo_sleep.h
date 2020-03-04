#pragma once

const uint64_t OneSecond = 1000000LL;
const uint64_t Time60 = 60LL;		/*60 min = 1 hour*/
const uint64_t OneMinute = OneSecond * Time60;	/*6000000 uS = 1 minute*/
const uint64_t OneHour = OneMinute * Time60;
const uint64_t OneDay = OneHour * 24;
// TODO andymule this number weirdly lets me wake up every 12 hours or so ? This hasn't proven true and needs more work
// TODO cant i enable a high resolution timer? or do i buy one
//const uint64_t SleepTimeMicroseconds = OneHour * 24LL;

const uint8_t wakePin = T5;
const uint8_t setupPin = T3;

bool SleepDriftWasTooFast = false;	// flag to handle math bc we're using uint64 and need to handle negatives
uint64_t SleepDriftCompensation = 0;

/*
NOTE:
======
Only RTC IO can be used as a source for external wake
source. They are pins: 0,2,4,12-15,25-27,32-39.
*/
// TODO wake oncaptouch, possible gestures?
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/ExternalWakeUp/ExternalWakeUp.ino
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/TouchWakeUp/TouchWakeUp.ino

//#define Threshold 100 /* Greater the value, more the sensitivity */ // needs to be more specific to GPIO
RTC_DATA_ATTR int bootCount = 0;
touch_pad_t touchPin;

enum WakeReason { HardReset, TimedRefresh, EnterSettings, RefreshNow };

/*
//https://github.com/Serpent999/ESP32_Touch_LED/blob/master/Touch_LED/Touch_LED.ino //WEIRD example of touch? might be useful
Touch Sensor Pin Layout
T0 = GPIO4, T1 = GPIO0, T2 = GPIO2, T3 = GPIO15, T4 = GPIO13, T5 = GPIO12, T6 = GPIO14, T7 = GPIO27, T8 = GPIO33, T9 = GPIO32 */

// RTC_DATA_ATTR marks variables to be saved across sleep
//static RTC_DATA_ATTR struct timeval sleep_enter_time;	// TODO exploit this for saving preferences??

void verbose_print_reset_reason(RESET_REASON reason)
{
	switch (reason)
	{
	case 1: Serial.println("Vbat power on reset"); break;
	case 3: Serial.println("Software reset digital core"); break;
	case 4: Serial.println("Legacy watch dog reset digital core"); break;
	case 5: Serial.println("Deep Sleep reset digital core"); break;
	case 6: Serial.println("Reset by SLC module, reset digital core"); break;
	case 7: Serial.println("Timer Group0 Watch dog reset digital core"); break;
	case 8: Serial.println("Timer Group1 Watch dog reset digital core"); break;
	case 9: Serial.println("RTC Watch dog Reset digital core"); break;
	case 10: Serial.println("Instrusion tested to reset CPU"); break;
	case 11: Serial.println("Time Group reset CPU"); break;
	case 12: Serial.println("Software reset CPU"); break;
	case 13: Serial.println("RTC Watch dog Reset CPU"); break;
	case 14: Serial.println("for APP CPU, reseted by PRO CPU"); break;
	case 15: Serial.println("Reset when the vdd voltage is not stable"); break;
	case 16: Serial.println("RTC Watch dog reset digital core and rtc module"); break;
	default: Serial.println("NO_MEAN");
	}
}


void EnableTouchpadWake()
{
	// TODO andymule detect current orientation with IMU? and set interupts accordingly if wake on tilt (dont do this)
	//esp_sleep_enable_ext1_wakeup(T1, ESP_EXT1_WAKEUP_ANY_HIGH);
	//esp_sleep_enable_ext0_wakeup(T1, ESP_EXT1_WAKEUP_ANY_HIGH);
	

// RESTING analog VALS on lilygo t5 v2.2
//  (values close to 0 mean touch detected)
//TO : 1
//T1 : 1
//T2 : 80
//T3 : 103 == 99 threshold
//T4 : 1
//T5 : 38 == 29 threshold
//T6 : 114
//T7 : 113
//T8 : 86
//T9 : 90

		/*while (true)
	{
		pp("TO:" + String(touchRead(T0)));
		pp("T1:" + String(touchRead(T1)));
		pp("T2:" + String(touchRead(T2)));
		pp("T3:" + String(touchRead(T3)));
		pp("T4:" + String(touchRead(T4)));
		pp("T5:" + String(touchRead(T5)));
		pp("T6:" + String(touchRead(T6)));
		pp("T7:" + String(touchRead(T7)));
		pp("T8:" + String(touchRead(T8)));
		pp("T9:" + String(touchRead(T9)));
		pp();
		pp();
		sleep(1);
	}*/

	/*
 * Set cycles that measurement operation takes
 * The result from touchRead, threshold and detection
 * accuracy depend on these values. Defaults are
 * 0x1000 for measure and 0x1000 for sleep.
 * With default values touchRead takes 0.5ms
 * */
	//void touchSetCycles(uint16_t measure, uint16_t sleep);

	touchAttachInterrupt(setupPin, NULL, 99);
	touchAttachInterrupt(wakePin, NULL, 29);
	esp_sleep_enable_touchpad_wakeup();
	//(uint32_t)esp_sleep_get_touchpad_wakeup_status();
}

void AtmoDeepSleep()
{
	// TODO blog says: It’s worth setting all pins to inputs before sleep, to ensure there are no active GPIO pull downs consuming power. 
	// TODO something on espressif manual about disabling pins to save power, might be same as above
	//gfx.powerOff();	// need to call this or else "they" say screen gets damaged from constant voltage
	gfx.hibernate(); // a better power off that lets us wake? idk if my board can do this, seems to glitch
	SPI.end();
	EnableTouchpadWake();	// actually allows wake on pin touch???
	uint64_t sleepTime = 0;
	if (SleepDriftWasTooFast)
		sleepTime = OneDay + SleepDriftCompensation; // woke up early from being too fast, so sleep longer next time
	else
		sleepTime = OneDay - SleepDriftCompensation;
	//pp((unsigned long int)SleepDriftCompensation);
	//pp((unsigned long int) sleepTime);
	//pp(SleepDriftWasTooFast);
	esp_sleep_enable_timer_wakeup(sleepTime);
	//Serial.print("sleep time: ");
	//uint64_t xx = sleepTime / 1000000000ULL;
	//if (xx > 0) Serial.print((long)xx);
	//Serial.println((long)(sleepTime - xx * 1000000000));
	//gettimeofday(&sleep_enter_time, NULL);
	//esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);

	esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF); //TODO read up on these configs
	esp_deep_sleep_start();
}


int get_wakeup_gpio_touchpad() {	// stole from internet. possible these aren't all real
	touch_pad_t pin;
	touchPin = esp_sleep_get_touchpad_wakeup_status();
	switch (touchPin)
	{
	case 0: return 4;
	case 1: return 0;
	case 2: return 2;
	case 3: return 15;
	case 4: return 13;
	case 5: return 12;
	case 6: return 14;
	case 7: return 27;
	case 8: return 33;
	case 9: return 32;
	default: return -1;
	}
	return -1;
}

WakeReason CheckResetReasonAndClearScreenIfNeeded()
{
	//uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
	//if (wakeupBit & GPIO_SEL_33) {
	//	// GPIO 33 woke up
	//}
	//else if (wakeupBit & GPIO_SEL_34) {
	//	// GPIO 34
	//}

	esp_sleep_wakeup_cause_t wakeup_reason;
	wakeup_reason = esp_sleep_get_wakeup_cause();
	switch (wakeup_reason) {
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TIMER: {
		Serial.println("Wake up from timer.");
		return WakeReason::TimedRefresh;
		break;
	}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_GPIO:
	{
		Serial.println("Wakeup caused by GPIO"); break;
	}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UART:
	{
		Serial.println("Wakeup caused by UART??"); break;
	}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TOUCHPAD:
	{
		int pad = get_wakeup_gpio_touchpad();
		Serial.println("Wakeup caused by touchpad");
		Serial.println("PAD:" + String(pad));
		if (pad == setupPin)
		{
			pp("entering setup");
			Prefs.putBool(PREF_VALID_BOOL, false); //invalidate location data // TODO indicate this on display
			return WakeReason::EnterSettings;
		}
		else
		{
			pp("entering refresh now");
			return WakeReason::RefreshNow;
		}
		break;
	}
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_ULP: Serial.println("Wakeup caused by ULP program"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT0: Serial.println("Wakeup caused by EXT0"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT1: Serial.println("Wakeup caused by EXT1"); break;
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UNDEFINED:
	default: {
		//gfx.eraseDisplay(true);
		//gfx.eraseDisplay();
		//gfx.setFullWindow();
		gfx.setFullWindow();
		gfx.fillScreen(GxEPD_WHITE);
		//gfx.setCursor(x, y);
		//gfx.print(HelloWorld);
		gfx.display(false); // full update
		//gfx.fillScreen(GxEPD_WHITE);
		//gfx.nextPage();
		//gfx.clearScreen();
		Serial.println("Wake from RESET or other");
		//Prefs.putBool(PREF_VALID_BOOL, false); //invalidate location data // TODO indicate this on display
		memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
		return WakeReason::HardReset;
	}
	}
}