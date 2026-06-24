# Board Migration — Lilygo T5 v2.2 → Waveshare ESP32‑S3‑Touch‑ePaper‑1.54

> Status: **implemented** — USB and battery. Atmo runs on the Waveshare board
> via the `s3_touch` PlatformIO env (the original T5 build is untouched),
> including the battery power latch (§3.1), panel/touch power management, and
> deep‑sleep wake. All hardware facts below are grounded in Waveshare's wiki and
> its official demo source:
> [docs.waveshare.com/ESP32‑S3‑ePaper‑1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54)
> and [github.com/waveshareteam/ESP32‑S3‑ePaper‑1.54](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54).
>
> Re‑flashing note: the firmware deep‑sleeps, which drops the USB‑Serial/JTAG
> port — see §8.1 for the BOOT‑button download‑mode procedure required to reflash.

---

## 1. The board

Waveshare's "ESP32‑S3‑ePaper‑1.54" is a small e‑paper AIoT dev board family. Per
the wiki there are four SKUs (touch and battery are the two options):

| SKU | Product | Touch | Battery |
| --- | --- | --- | --- |
| 32298 | ESP32‑S3‑ePaper‑1.54 | no | yes |
| 32299 | ESP32‑S3‑ePaper‑1.54‑EN | no | no |
| **34211** | **ESP32‑S3‑Touch‑ePaper‑1.54** | **yes** | **yes** |
| 34212 | ESP32‑S3‑Touch‑ePaper‑1.54‑EN | yes | no |

**This project targets SKU 34211** (FT6336 capacitive touch + onboard Li‑ion
charging).

### Hardware revisions (matters for flash/PSRAM)

The wiki calls out two PCB revisions; they are **not** program‑interchangeable:

- **V1** — `ESP32‑S3FH4R2`, 4 MB flash + 2 MB PSRAM.
- **V2** — `ESP32‑S3‑PICO‑1‑N8R8`, 8 MB flash + 8 MB PSRAM, plus optimized
  whole‑board sleep power. Phased in from 2025‑11‑01; marked "V2" on the back /
  top‑left silkscreen.

**The unit on hand is V2.** `esptool` confirms it: `ESP32‑S3‑PICO‑1 (LGA56)
rev v0.2`, `Embedded Flash 8MB (GD)` (eFuse flash type **quad**), `Embedded
PSRAM 8MB`, native **USB‑Serial/JTAG**.

### Panel

1.54" **200×200 black/white**, SPI, controller **GDEY0154D67** (SSD1681 class).
In GxEPD2 this is `GxEPD2_154_D67`.

---

## 2. Pin map (from the Waveshare wiki GPIO table)

ESP32‑S3 RTC‑capable GPIOs are the contiguous range **GPIO0–GPIO21**. Every EPD
line below is inside that range, which is what makes a future ULP‑driven refresh
even theoretically possible (see §7).

### E‑paper (SPI) + touch

| Signal | GPIO | In RTC range (0–21)? | Notes |
| --- | --- | --- | --- |
| **EPD_PWR** (EPD3V3_EN) | 6 | ✓ | Panel rail enable — **ACTIVE‑LOW**, see §3 |
| EPD_BUSY | 8 | ✓ | busy = HIGH |
| EPD_RST | 9 | ✓ | |
| EPD_DC | 10 | ✓ | |
| EPD_CS | 11 | ✓ | |
| EPD_SCLK | 12 | ✓ | |
| EPD_SDI (MOSI) | 13 | ✓ | panel is write‑only; no MISO |
| EPD_TP_RST | 7 | ✓ | FT6336 touch reset |
| EPD_TP_INT | 21 | ✓ | FT6336 touch interrupt |
| EPD_TP I²C | SDA 47 / SCL 48 | ✗ | shared I²C bus (see below) |

### Everything else (for reference)

| Function | GPIO | Notes |
| --- | --- | --- |
| BOOT / Key1 | 0 | strapping pin; the only usable user button |
| Reserved header | 1, 2, 3 | 2×6 expansion header |
| BAT_ADC | 4 | VBAT = VADC × 2 (ADC1 / RTC) |
| RTC_INT (PCF85063) | 5 | |
| BAT_Control (VBAT_PWR) / BAT_KEY (PWR btn) | 17 / 18 | PWR soft‑latch — **GPIO17 must be held high on battery**, see §3.1 |
| USB D‑ / D+ (native) | 19 / 20 | USB‑Serial/JTAG |
| I²C bus (RTC + SHTC3 + ES8311 + touch) | SDA 47 / SCL 48 | **outside** RTC range |
| Audio ES8311 (I²S) | MCLK 14, SCLK 15, ASDOUT 16, LRCK 38, DSDIN 45, PA_EN 42, PA_CTRL 46 | |
| TF card (SDIO) | CLK 39, MISO 40, MOSI 41 | |
| UART0 | TX 43, RX 44 | |
| RTC | PCF85063ATL | I²C `0x51` |
| Temp/Humidity | SHTC3 | I²C `0x70` |
| Charger | ETA6098 | single‑cell Li‑ion |
| 3.3V DC‑DC | MP1605 | |

> The PCF85063 RTC, SHTC3, and FT6336 touch all sit on the I²C bus at GPIO47/48,
> which is **outside** the RTC GPIO range — so none of them are reachable by the
> ULP during deep sleep. That's fine: they're for the main core.

---

## 3. The EPD power‑enable gotcha (root cause of the first "blank screen")

`EPD_PWR` / **GPIO6** (the wiki's `EPD3V3_EN`, "E‑Paper power switch / 3.3V
supply enable") gates the panel's `EPD3V3` rail through an **AO3401 P‑channel
high‑side MOSFET**. A P‑FET high‑side switch driven from the GPIO is
**active‑LOW**:

- **GPIO6 = LOW → panel powered.**
- GPIO6 = HIGH → panel display rail OFF.

The trap: the panel's *digital* logic is powered from the always‑on 3V3 rail, so
with GPIO6 HIGH the controller still boots, **`BUSY` still toggles, and refreshes
still "complete"** (~1.29 s) — but the high‑voltage rail that physically moves
the e‑paper is off, so the image never changes and the panel keeps showing its
last frame. Confirmed two ways: with GPIO6 driven LOW the barebones test (§6)
immediately rendered, **and** Waveshare's own demo BSP does the same —

```cpp
// 02_Example/Arduino/11_RTC_Sleep_Test/src/power/board_power_bsp.cpp
void POWEER_EPD_ON()  { gpio_set_level(epd_power_pin, 0); }  // LOW  = panel ON
void POWEER_EPD_OFF() { gpio_set_level(epd_power_pin, 1); }  // HIGH = panel OFF
```

(Note: AI/web summaries that say "drive GPIO6 HIGH" are wrong for this board —
they likely describe a different/older variant. The wiki warns example programs
aren't interchangeable between V1/V2.)

## 3.1 Battery power latch — GPIO17 (REQUIRED for battery; implemented)

The board has a **soft power‑latch** for battery use, and the firmware must hold
it. From Waveshare's demo (`board_power_bsp.cpp` + `user_app.cpp`):

- `VBAT_PWR` / `BAT_Control` = **GPIO17**. `VBAT_POWER_ON()` drives it **HIGH**
  to keep the battery rail latched on; `VBAT_POWER_OFF()` drives it LOW to power
  the board off.
- The `PWR` button = `BAT_KEY` = **GPIO18** (active‑low). It physically powers
  the board on momentarily; firmware then latches GPIO17 high to stay on.
- Before deep sleep, the latch is held through sleep with
  `rtc_gpio_hold_en(GPIO17)` (and released with `gpio_hold_dis(GPIO17)` on wake);
  otherwise the board powers down mid‑sleep and never wakes.

Waveshare's boot order (their `user_app_init`):

```cpp
board_div.VBAT_POWER_ON();   // GPIO17 HIGH  -> latch battery power on
board_div.POWEER_EPD_ON();   // GPIO6  LOW   -> panel rail on
// ... EPD init ...
// before deep sleep:  rtc_gpio_hold_en(GPIO17); esp_deep_sleep_start();
// if woken by PWR(GPIO18): gpio_hold_dis(GPIO17); VBAT_POWER_OFF();  // power off
```

**Why it matters here:** on USB the board is powered from VBUS regardless of
GPIO17; on **battery**, without driving/holding GPIO17 the board powers off as
soon as PWR is released and cannot survive deep sleep.

**Implemented** (`SleepManager::initBoardPower()`, called first in `setup()`):
drive GPIO17 HIGH and `gpio_hold_en` + `gpio_deep_sleep_hold_en` so it stays
latched through every wake/sleep cycle. A PWR‑button (GPIO18) deep‑sleep wake is
detected in `checkWakeReason()` and routed to `SleepManager::powerOff()`, which
`gpio_hold_dis`es GPIO17 and drives it LOW to power the board off (then sleeps so
USB use can still resume with a button press). The panel rail (GPIO6, LOW) and
FT6336 reset (GPIO7, LOW) use the same hold mechanism so the panel stays powered
+ hibernating and touch stays parked off through sleep — see §5.5.

---

## 4. What the code needed (and what was portable as‑is)

| Area | T5 v2.2 | Waveshare S3 | Port effort |
| --- | --- | --- | --- |
| MCU | classic ESP32 (`esp32dev`) | ESP32‑S3 (`esp32s3`) | new PlatformIO env |
| Panel | 2.9" 296×128 `GxEPD2_290` | 1.54" 200×200 `GxEPD2_154_D67` | conditional panel class |
| SPI pins | bus defaults (18/23/19…) | 12/13 + manual bind | bind in `Display::begin()` |
| Panel power | always on | gated, **active‑low**, GPIO6 | drive LOW in `begin()` |
| Touch | n/a | FT6336 on always‑on 3V3 | hold in reset, see §5 |
| Wake | ext1 button (GPIO37) | ext1 button (GPIO0 / BOOT) | retarget GPIO |
| Sleep schedule, RTC clock, cache | chip‑agnostic | same | none |

Wake was already a generic **ext1 button interrupt** (not classic‑ESP32 touch
pads), so it ported by just changing the GPIO number. The forecast cache, hourly
+ quiet‑hours schedule, drift‑free RTC clock, and `RTC_DATA_ATTR` state are all
chip‑agnostic and carried over unchanged.

---

## 5. The implemented port

All Waveshare‑specific code is behind the **`BOARD_WAVESHARE_S3_154_TOUCH`**
build flag, so the T5 build is byte‑for‑byte unchanged.

### 5.1 `platformio.ini` — the `s3_touch` env

```ini
[env:s3_touch]
board = esp32-s3-devkitc-1
board_build.mcu = esp32s3
board_build.flash_mode = qio
board_build.flash_size = 8MB
board_upload.flash_size = 8MB
build_flags =
	-DDEBUG_BUILD
	-DBOARD_WAVESHARE_S3_154_TOUCH
	-DARDUINO_USB_MODE=1        ; route Serial to the native USB‑Serial/JTAG
	-DARDUINO_USB_CDC_ON_BOOT=1
```

- **Flash:** quad, 8 MB (matches the V2 eFuse reading).
- **PSRAM:** intentionally **left disabled**. The firmware never needed PSRAM
  (it ran on the PSRAM‑less classic ESP32), and a mismatched PSRAM mode is a
  common boot‑loop cause. Enable later (`memory_type` + `BOARD_HAS_PSRAM`) only
  if a feature wants it.
- `min_spiffs.csv` (a 4 MB table) is inherited from the shared `[env]`; it fits
  the 8 MB part with room to spare.

### 5.2 `include/Config.h` — board pins

The `pins` namespace selects the S3 map under the flag. Key entries:

```cpp
constexpr int EPD_PWR = 6;    // EPD3V3_EN: ACTIVE‑LOW (AO3401 P‑FET) -> LOW = on
constexpr int EPD_CS = 11;
constexpr int EPD_DC = 10;
constexpr int EPD_RST = 9;
constexpr int EPD_BUSY = 8;
constexpr int SPI_SCK = 12;
constexpr int SPI_MISO = -1;  // panel is write‑only
constexpr int SPI_MOSI = 13;
constexpr int EPD_TP_RST = 7; // FT6336 touch reset (held low in sleep)

constexpr int WAKE_BUTTON = 0;   // BOOT — the only usable user button
constexpr int SETUP_BUTTON = 2;  // free RTC GPIO, INPUT_PULLUP (optional jumper)
```

Only the BOOT button is wired as a user key, so it doubles as wake/refresh.
There's no second physical button, so `SETUP_BUTTON` points at a free reserved
header GPIO read with an internal pull‑up: it reads "not held" unless someone
ties it to GND, and first‑run setup is still reached automatically whenever no
WiFi is stored.

### 5.3 `src/display/Display.h` — conditional panel class

```cpp
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> gfx_{
      GxEPD2_154_D67(pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY)};
#else
  GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> gfx_{ /* T5 2.9" */ };
#endif
```

### 5.4 `src/display/Display.cpp` — power rail + SPI bind in `begin()`

```cpp
#if defined(BOARD_WAVESHARE_S3_154_TOUCH)
  pinMode(pins::EPD_PWR, OUTPUT);
  digitalWrite(pins::EPD_PWR, LOW);   // active‑low: LOW powers the panel (§3)

  // FT6336 sits on the always‑on 3V3 rail, so park it in reset to save battery.
  pinMode(pins::EPD_TP_RST, OUTPUT);
  digitalWrite(pins::EPD_TP_RST, LOW);
  gpio_hold_en((gpio_num_t)pins::EPD_TP_RST);
  gpio_deep_sleep_hold_en();           // keep RST low through deep sleep

  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, pins::EPD_CS);
#endif
```

SPI is bound here (not in `main()`) because the S3 panel pins aren't the bus
defaults — and the default S3 `MOSI` is GPIO11, which is our CS. The core's
`SPI.begin()` early‑returns if the bus is already initialized, so this custom
bind wins over the no‑arg `SPI.begin()` that GxEPD2's `init()` calls internally.
`main.cpp` therefore guards its own `SPI.begin()` to non‑S3 boards.

All non‑weather screens (connection / offline / no‑WiFi / settings‑saved) use a
**full‑window** refresh and auto‑center/scale their text to the panel size;
partial refreshes don't reliably latch on this SSD1681, and the old text was
hard‑positioned for 296×128.

### 5.5 `src/power/SleepManager.cpp` — power latch + wake + touch + ULP guards

- **`initBoardPower()`** (called first in `setup()`): latches `VBAT_PWR`/GPIO17
  HIGH and holds it through deep sleep so the board stays powered on battery
  (§3.1). No‑op effect on the T5.
- **ext1 wake** targets `WAKE_BUTTON`/GPIO0 (BOOT) and, on the S3, also
  `PWR_BUTTON`/GPIO18, using `ESP_EXT1_WAKEUP_ANY_LOW` (the T5 keeps a single‑pin
  `ALL_LOW`).
- **Button behavior (S3):**
  - **BOOT, short tap** → wake + immediate refresh (`RefreshNow`).
  - **BOOT, long press (~3 s)** → open the at.mo setup portal (`EnterSettings`).
    `setupButtonHeld()` polls GPIO0 after wake and returns true only if held past
    the threshold (returns immediately on release, so a refresh tap stays snappy).
    Setup mode stays awake, so it doubles as the **USB re‑flash window** (§8.1).
  - **PWR press** → `powerOff()`: unlatches GPIO17 → board powers off on battery.
- The classic‑only `#include "esp32/ulp.h"` and the `RTC_SLOW_MEM` memset are
  guarded out for the S3 (the S3 ULP isn't used by this build).
- The panel rail (GPIO6 LOW) and FT6336 reset (GPIO7 LOW) stay latched across
  deep sleep via the GPIO hold set in `Display::begin()`, so the panel stays
  powered + hibernating (image/RAM retained) and touch can't drain the battery.

### 5.6 Setup‑portal interactions & robustness refinements (S3)

Setup mode stays awake (it hosts the AP + web server), which is also the easy
USB re‑flash window (§8.1). Button handling while in setup (`main.cpp` `loop()`):

- **Tap BOOT** → resume normal operation (only if WiFi is already saved). Requires
  a full release→press→release so the long‑press that opened setup doesn't bounce
  straight out, and so GPIO0 is HIGH at the `ESP.restart()` (otherwise it would
  strap the chip into ROM download mode).
- **PWR** → `powerOff()` so a board left in setup on battery doesn't sit hosting
  the AP and drain.

Code‑review fixes made during this pass (apply to both boards unless noted):

- **Failed daily fetch now recovers on the backoff schedule** instead of waiting
  a full day: `SleepManager::fetchRetryPending()` (failure streak > 0) forces a
  fetch on the next wake, and a fetch that fails while cache exists now sleeps via
  `deepSleepRetry()` (still showing cached data) rather than the daily schedule.
- **Hourly window aligned to the Date‑synced clock**: the hourly slice start index
  is computed from `out.current.utcEpoch` (the same time fed to `setClock`), not
  the coarse JSON `current.time`, so the curve's left edge tracks the on‑screen
  clock (`OpenMeteoProvider`).
- **S3 error screen** says "Hold BOOT to open setup" (no dedicated setup button).

---

## 6. Barebones bring‑up test (`s3_test` env)

A flag‑gated (`BAREBONES_EPD_TEST`) test in `main.cpp` bypasses the entire app —
Settings, WiFi, layouts — and draws a fixed pattern (border, solid block, corner
marks, "HELLO") straight to the panel with verbose logging. It was the tool that
isolated the active‑low power bug from the app logic.

```bash
~/.pio-venv/bin/pio run -e s3_test -t upload --upload-port /dev/cu.usbmodem2101
```

Keep it as a future bring‑up aid, or delete the `s3_test` env + the
`#if defined(BAREBONES_EPD_TEST)` blocks once no longer needed.

---

## 7. ULP‑driven hourly refresh — analyzed and deliberately NOT pursued

The board makes ULP‑driven refresh *physically* possible (SPI panel; every EPD
pin, incl. the power enable, on RTC GPIOs). After a deep analysis against this
project's actual use case — an **hourly clock that changes each hour** — it is
the wrong investment. Three independent reasons, any one of which is decisive:

**1. It doesn't eliminate the hourly CPU wake (zero benefit for a changing clock).**
The whole point would be to redraw without waking the main cores. But each hour
the *content changes* (the clock reads a new hour; the 24 h curve rolls). To
produce the new frame the ULP would have to either:
- read the time — but the PCF85063 RTC and the only I²C bus are on GPIO47/48,
  **outside** the RTC domain, so the ULP can't read them; or
- render text/curves itself — far beyond a bit‑bang blit; or
- replay a pre‑rendered frame — but only **one** 200×200×1bpp frame (5000 B)
  fits in the S3's **8 KB `RTC_SLOW_MEM`**, which the ULP program must also share.
So the CPU must wake each hour to render the next frame regardless → the ULP
cannot remove the hourly wake → **no energy saved**. (A static image the ULP
could repaint would benefit; our changing clock cannot.)

**2. Our stack doesn't support it; it would force an ESP‑IDF rewrite.**
ULP‑RISC‑V needs the ESP‑IDF build system. Arduino / PlatformIO‑Arduino do **not**
support it (confirmed: PlatformIO community reports, `ddekanski/esp32_riscv_ulp_test`
"neither Arduino IDE nor PlatformIO support RISC‑V ULP", and our own toolchain is
arduino‑esp32 2.0.17). There is also **no ULP SPI API or example** — Espressif
issue **IDFGH‑15605** is an *open feature request*; you'd hand‑write bit‑banged
SPI in RISC‑V. Adopting IDF means re‑homing WiFi, HTTPClient, GxEPD2, Preferences,
and the WebServer captive portal — a ground‑up rewrite for the whole app.

**3. The power math is marginal even in the best case.**
Measured ULP draw is ~**416 µA while running** vs ~**34 µA** idle deep sleep
(`ddekanski`). A refresh needs the ULP active for the panel's full ~1.3 s BUSY
wait, and the CPU wake it would (hypothetically) replace is only ~0.008 mAh.
The daily energy budget is dominated by the once‑a‑day WiFi fetch and baseline
deep‑sleep current, not the brief hourly render — so even an ideal ULP path would
shave a low‑single‑digit percentage, for a massive rewrite and ongoing fragility.

**Decision:** keep the brief hourly **main‑core** wake (it's already efficient —
~1–2 s awake, then deep sleep with all rails/holds managed, §3.1/§5.5). Revisit
the ULP only if the product ever shows **static** content for long stretches, or
if Espressif ships first‑class ULP‑SPI + Arduino support. If pursued anyway, the
prerequisite is an ESP‑IDF (or arduino‑as‑component) migration tracked as its own
project — not a drop‑in.

> References: ESP‑IDF ULP‑RISC‑V guide (esp32s3) `docs.espressif.com`; ULP‑SPI
> feature request `github.com/espressif/esp-idf/issues/16235` (IDFGH‑15605);
> `github.com/ddekanski/esp32_riscv_ulp_test` (ULP current measurements + the
> Arduino/PlatformIO support gap).

---

## 8. Build / flash / monitor

```bash
# Build + flash the app
~/.pio-venv/bin/pio run -e s3_touch -t upload --upload-port /dev/cu.usbmodem2101
```

Toolchain note: the stock PlatformIO `espressif32` platform here bundles
**arduino‑esp32 2.0.17**, which builds and runs this firmware fine. (Waveshare's
wiki asks for arduino‑esp32 3.3.0+, but that's for *their* LVGL/audio/touch
examples — not this minimal GxEPD2 build.)

### 8.1 Download mode — REQUIRED to re‑flash the production firmware

Because the production firmware **deep‑sleeps between updates**, and the
ESP32‑S3's **native USB‑Serial/JTAG turns off in deep sleep**, the
`/dev/cu.usbmodem…` port **disappears whenever the board is asleep**. esptool
then can't auto‑reset it, so uploads fail with *"Could not open … the port is
busy or doesn't exist."* This is expected, not a fault.

**Easiest path — the setup‑portal flash window:** long‑press the **BOOT** button
(~3 s) to enter the at.mo setup portal. The device stays awake there, so the
USB‑Serial/JTAG port stays enumerated and you can flash normally (no download
mode needed). Use this whenever the firmware is healthy.

**Fallback — ROM download mode** (first flash, or if the firmware is broken /
sleeping). Per Waveshare's wiki: "hold the BOOT button to power on again to enter
download mode":

1. Unplug the battery (if attached) and the USB cable so the board is fully off.
2. **Hold the BOOT button.**
3. Plug in USB while still holding BOOT (equivalently: hold BOOT, tap RST, then
   release BOOT).
4. Release BOOT. `/dev/cu.usbmodem…` now appears and stays put (the ROM
   bootloader doesn't sleep).
5. Run the upload command above. esptool hard‑resets the board to run afterward.

Tip: confirm the port came up before flashing — `ls /dev/cu.*`.

### 8.2 Monitoring serial

The board enumerates as native USB‑Serial/JTAG (e.g. `/dev/cu.usbmodem2101`), but
only while it's **awake** (boot → fetch/render → just before deep sleep).
`pio device monitor` needs a TTY; in a non‑TTY shell, read the port with a short
pyserial script (toggle RTS to reset, then read) right after a reset/wake.

### 8.3 Settings / NVS

WiFi creds + preferences live in NVS and survive a reflash. To force first‑run
setup, hold the setup jumper, or wipe NVS with `pio run -e s3_touch -t erase`
and re‑upload.

### 8.4 Running on battery

After flashing, unplug USB, connect the Li‑ion battery, and **press/hold the PWR
button** to power on (the firmware then latches `VBAT_PWR`/GPIO17 to stay on —
§3.1). A single PWR press while running powers the board off (unlatch).

---

## 9. References

- Waveshare wiki — [ESP32‑S3‑ePaper‑1.54](https://docs.waveshare.com/ESP32-S3-ePaper-1.54)
  (board overview, V1/V2 modules, SKUs, GPIO table, onboard resources).
- Waveshare demo repo —
  [github.com/waveshareteam/ESP32‑S3‑ePaper‑1.54](https://github.com/waveshareteam/ESP32-S3-ePaper-1.54).
  The authoritative pin/power/sleep reference is
  `02_Example/Arduino/11_RTC_Sleep_Test` (`user_config.h`, `board_power_bsp.cpp`,
  `user_app.cpp`) — confirms EPD_PWR active‑low and the GPIO17 VBAT latch.
- Waveshare schematic — `ESP32‑S3‑Touch‑ePaper‑1.54‑Schematic.pdf`
  (EPD3V3_EN / AO3401 power switch, J10 panel connector).
- GxEPD2 (ZinggJM) — `GxEPD2_154_D67` driver for the GDEY0154D67 panel.
- ESP‑IDF ULP‑RISC‑V programming guide & sleep modes (esp32s3) —
  `docs.espressif.com`.
