#pragma once

const uint64_t OneSecond = 1000000LL;
const uint64_t Time60 = 60LL;		/*60 min = 1 hour*/
const uint64_t OneMinute = OneSecond * Time60;	/*6000000 uS = 1 minute*/
const uint64_t OneHour = OneMinute * Time60;
const uint64_t OneDay = OneHour * 24;

const uint8_t wakePin = T5;	// GPIO 12
const uint8_t setupPin = T3;	// GPIO 15

RTC_DATA_ATTR bool SleepDriftWasTooFast = false;
RTC_DATA_ATTR uint64_t SleepDriftCompensation = 0;
RTC_DATA_ATTR time_t lastServerTime = 0;
RTC_DATA_ATTR uint64_t lastSleepDuration = 0;

// Only RTC IO can be used for external wake: pins 0,2,4,12-15,25-27,32-39
RTC_DATA_ATTR int bootCount = 0;
touch_pad_t touchPin;

enum WakeReason { HardReset, TimedRefresh, EnterSettings, RefreshNow };

void verbose_print_reset_reason(RESET_REASON reason)
{
	switch (reason)
	{
	case 1: pp("Vbat power on reset"); break;
	case 3: pp("Software reset digital core"); break;
	case 4: pp("Legacy watch dog reset digital core"); break;
	case 5: pp("Deep Sleep reset digital core"); break;
	case 6: pp("Reset by SLC module, reset digital core"); break;
	case 7: pp("Timer Group0 Watch dog reset digital core"); break;
	case 8: pp("Timer Group1 Watch dog reset digital core"); break;
	case 9: pp("RTC Watch dog Reset digital core"); break;
	case 10: pp("Instrusion tested to reset CPU"); break;
	case 11: pp("Time Group reset CPU"); break;
	case 12: pp("Software reset CPU"); break;
	case 13: pp("RTC Watch dog Reset CPU"); break;
	case 14: pp("for APP CPU, reseted by PRO CPU"); break;
	case 15: pp("Reset when the vdd voltage is not stable"); break;
	case 16: pp("RTC Watch dog reset digital core and rtc module"); break;
	default: pp("NO_MEAN");
	}
}


void EnableTouchpadWake()
{
	// Lilygo T5 v2.2 resting thresholds: T3=103 (threshold 99), T5=38 (threshold 29)
	touchAttachInterrupt(setupPin, NULL, 99);
	touchAttachInterrupt(wakePin, NULL, 29);
	esp_sleep_enable_touchpad_wakeup();
}

void AtmoDeepSleep()
{
	Prefs.end();
	gfx.hibernate();
	SPI.end();
	EnableTouchpadWake();
	uint64_t sleepTime;
	if (SleepDriftWasTooFast)
		sleepTime = OneDay + SleepDriftCompensation;
	else
		sleepTime = OneDay - SleepDriftCompensation;
	lastSleepDuration = sleepTime;
	esp_sleep_enable_timer_wakeup(sleepTime);
	esp_deep_sleep_start();
}


int get_wakeup_gpio_touchpad() {
	touchPin = (touch_pad_t)esp_sleep_get_touchpad_wakeup_status();
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
}

WakeReason CheckResetReasonAndClearScreenIfNeeded()
{
	esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
	switch (wakeup_reason) {
	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TIMER:
		pp("Wake up from timer.");
		return WakeReason::TimedRefresh;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_TOUCHPAD:
	{
		int pad = get_wakeup_gpio_touchpad();
		pp("Wakeup caused by touchpad");
		pp("PAD:" + String(pad));
		if (pad == setupPin)
		{
			pp("entering setup");
			Prefs.putBool(PREF_VALID_BOOL, false);
			return WakeReason::EnterSettings;
		}
		else
		{
			pp("entering refresh now");
			return WakeReason::RefreshNow;
		}
	}

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_GPIO:
		pp("Wakeup caused by GPIO");
		return WakeReason::RefreshNow;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_ULP:
		pp("Wakeup caused by ULP program");
		return WakeReason::RefreshNow;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT0:
		pp("Wakeup caused by EXT0");
		return WakeReason::RefreshNow;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_EXT1:
		pp("Wakeup caused by EXT1");
		return WakeReason::RefreshNow;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UART:
		pp("Wakeup caused by UART");
		return WakeReason::RefreshNow;

	case esp_sleep_wakeup_cause_t::ESP_SLEEP_WAKEUP_UNDEFINED:
	default:
		gfx.setFullWindow();
		gfx.fillScreen(GxEPD_WHITE);
		gfx.display(false);
		pp("Wake from RESET or other");
		memset(RTC_SLOW_MEM, 0, CONFIG_ULP_COPROC_RESERVE_MEM);
		return WakeReason::HardReset;
	}
}