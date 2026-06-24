# Hardware Reference — Waveshare ESP32-S3-Touch-ePaper-1.54

Quick, complete notes for building **any** firmware on this board. Sourced from
Waveshare's wiki + demo repo and verified on-device. Companion docs:
`BOARD_MIGRATION.md` (how Atmo was ported) and `ULP_REFRESH.md` (deferred ULP plan).

## Board identity

- **Product:** Waveshare `ESP32-S3-Touch-ePaper-1.54`, SKU **34211** (touch + Li-ion battery).
- **Module (V2):** `ESP32-S3-PICO-1-N8R8` — **8 MB flash (quad)** + **8 MB PSRAM**, native **USB-Serial/JTAG**. (V1 was `ESP32-S3FH4R2`, 4 MB/2 MB — example programs are *not* interchangeable.) Verify "V2" silkscreen; this repo targets V2.
- **Panel:** 1.54" **200×200 B/W**, SPI, controller **GDEY0154D67** (SSD1681) → GxEPD2 class `GxEPD2_154_D67`.

## GPIO map (authoritative)

| Function | GPIO | Notes |
| --- | --- | --- |
| EPD_PWR (EPD3V3_EN) | 6 | **ACTIVE-LOW** (AO3401 P-FET): LOW = panel on, HIGH = off |
| EPD_BUSY | 8 | busy = HIGH |
| EPD_RST | 9 | |
| EPD_DC | 10 | |
| EPD_CS | 11 | |
| EPD_SCLK | 12 | |
| EPD_SDI (MOSI) | 13 | panel is write-only (no MISO) |
| EPD_TP_RST | 7 | FT6336 touch reset |
| EPD_TP_INT | 21 | FT6336 touch interrupt |
| I²C SDA / SCL | 47 / 48 | shared: RTC + SHTC3 + ES8311 + touch. **Outside RTC domain** |
| BAT_ADC | 4 | VBAT = VADC × 2 (ADC1 / RTC) |
| RTC_INT (PCF85063) | 5 | |
| VBAT_PWR (BAT_Control) | 17 | **Battery latch: drive HIGH + hold to stay powered** |
| PWR button (BAT_KEY) | 18 | side button, active-low |
| BOOT button | 0 | side button, active-low; strapping pin |
| USB D- / D+ | 19 / 20 | native USB-Serial/JTAG |
| UART0 TX / RX | 43 / 44 | |
| Audio ES8311 (I²S) | MCLK 14, SCLK 15, ASDOUT 16, LRCK 38, DSDIN 45, PA_EN 42, PA_CTRL 46 | codec @ I²C `0x18` |
| TF card (SDIO) | CLK 39, MISO 40, MOSI 41 | FAT32 |
| Free expansion header | 1, 2, 3 | 2×6 2.54 mm header |

All EPD control/SPI pins are in the S3 RTC range (GPIO0–21).

## Power rules (the non-obvious ones — get these wrong and it looks "dead")

1. **Panel power is active-LOW** (GPIO6). Drive **LOW** before `gfx.init()`. Driving HIGH leaves the controller logic alive (BUSY responds, refreshes "complete") but the display never physically changes.
2. **Battery latch (GPIO17):** drive **HIGH** early in `setup()` and hold it across deep sleep (`gpio_hold_en` + `gpio_deep_sleep_hold_en`), or the board powers off on battery when PWR is released. No-op on USB. To power off: `gpio_hold_dis(17)` then drive LOW.
3. **FT6336 touch is on the always-on 3V3 rail** (not the EPD rail). If unused, hold `EPD_TP_RST` (GPIO7) LOW (with hold) so it doesn't drain the battery.
4. SPI is **not** on the bus defaults; the S3 default `MOSI` is GPIO11 (= our CS). Bind explicitly: `SPI.begin(12, -1, 13, 11)` before init.

## Buttons (as wired in this firmware — change freely)

- **BOOT (GPIO0):** tap = wake/refresh; long-press ~3 s = enter setup. Wakes via ext1.
- **PWR (GPIO18):** wakes via ext1; used here to power off (unlatch GPIO17).
- ext1 wake uses `ESP_EXT1_WAKEUP_ANY_LOW` with the mask of both buttons.

## Onboard peripherals available for new experiences

| Device | Bus / addr | Use |
| --- | --- | --- |
| PCF85063 RTC | I²C `0x51`, INT GPIO5 | timekeeping/alarms (CPU only — not ULP-reachable) |
| SHTC3 temp/humidity | I²C `0x70` | ambient sensing |
| ES8311 codec + mic + speaker | I²S (see table), `0x18` | audio in/out (PA_EN GPIO42) |
| FT6336 capacitive touch | I²C `47/48`, INT 21, RST 7 | touch input |
| TF card slot | SDIO 39/40/41 | storage (FAT32) |
| Battery ADC | GPIO4 | `VBAT = analogReadMilliVolts(4) * 2` |

## Build / flash / monitor

- **Toolchain:** PlatformIO `espressif32` platform, **arduino-esp32 2.0.17** (sufficient; Waveshare's 3.3.0+ note is only for their LVGL/audio/touch demos). `pio` lives at `~/.pio-venv/bin/pio`.
- **Env** (`platformio.ini` `[env:s3_touch]`): `board = esp32-s3-devkitc-1`, `board_build.mcu = esp32s3`, `flash_mode = qio`, `flash_size = 8MB`, flags `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`. PSRAM left disabled (enable only if needed). Board-specific code is behind `-DBOARD_WAVESHARE_S3_154_TOUCH`.

```bash
~/.pio-venv/bin/pio run -e s3_touch -t upload --upload-port /dev/cu.usbmodem2101
```

- **The firmware deep-sleeps**, and the native USB-Serial/JTAG powers down in sleep → the `/dev/cu.usbmodem…` port **disappears**. Two ways to flash:
  - **Easy:** while a build that stays awake is running (e.g. setup/AP mode), the port stays up — just upload.
  - **Reliable / first flash / broken firmware — ROM download mode:** battery out, **hold BOOT**, plug USB, release BOOT. Port appears; upload; esptool resets to run.
- **Monitor:** `pio device monitor` needs a TTY; in a non-TTY shell read the port with a short pyserial script (toggle RTS to reset, then read) — only works while the board is awake.

## Source layout (where the board-specific bits live)

- `include/Config.h` — `pins` namespace (S3 map behind `BOARD_WAVESHARE_S3_154_TOUCH`).
- `src/display/Display.{h,cpp}` — `GxEPD2_154_D67`, panel power-on (active-low) + SPI bind + touch-reset hold in `begin()`.
- `src/power/SleepManager.{h,cpp}` — deep sleep, ext1 wake, VBAT latch (`initBoardPower`), `powerOff`, GPIO holds.
- `src/main.cpp` — orchestration; gated `BAREBONES_EPD_TEST` panel bring-up (env `s3_test`) for isolating wiring/driver issues.
