# E‑Paper Dev Board Migration — Research & Porting Plan

> Status: **planning only** (no code changes applied yet).
> Goal: move the Atmo weather display off the Lilygo T5 v2.2 onto a board that
> better supports the priorities below, with a clear porting path.

---

## 1. Priorities (as stated)

In rough priority order:

1. **Correctly‑wired e‑ink so it can be driven from the ULP** — i.e. refresh the
   panel while the main CPU cores are in deep sleep.
2. **Onboard battery charging.**
3. **Correct deep sleep + wake.**
4. **Good power management** (PMIC quality).
5. **RTC** — nice to have, not required.

Explicitly *not* important: native buttons / touch integration. Buttons and
touchpads are trivial to add later; the items above are not.

This reordering matters: it makes the **ULP‑drivable e‑ink** the binding
constraint, and that single requirement eliminates several "obvious" boards.

---

## 2. What the current code depends on

| Area | Detail | Source |
| --- | --- | --- |
| MCU | Classic ESP32 (`board = esp32dev`) | `platformio.ini` |
| Panel | 2.9" B/W via GxEPD2 (`GxEPD2_290`, ~296×128, SSD1680‑class) | `src/display/Display.h` |
| Power | Battery + deep sleep, ~daily wake, drift compensation in RTC mem | `src/power/SleepManager.cpp` |
| Wake | ESP32 **native touch pads** (`touchAttachInterrupt` + `esp_sleep_enable_touchpad_wakeup`), T5/GPIO12 = refresh, T3/GPIO15 = setup | `include/Config.h`, `src/power/SleepManager.cpp` |

Everything except the touch‑pad wake is portable. The touch‑pad wake is the only
genuinely board‑specific piece of code — and it has to be rewritten regardless of
the target, because almost every integrated e‑paper board exposes physical
buttons (EXT0/EXT1 wake) rather than native touch pads.

---

## 3. The ULP constraint (why this drives the whole decision)

To refresh an e‑paper panel from the ULP while the main cores sleep, **two**
things must both be true:

### 3.1 The chip needs a capable ULP

- **Classic ESP32** has only the **ULP‑FSM** — a tiny assembly state machine, no
  practical bit‑banged SPI. Not viable for driving a panel.
- **ESP32‑S3** has the **ULP‑RISC‑V** — programmable in C, ~17.5 MHz, can stay
  powered in deep sleep, and can access RTC GPIO / RTC I²C / SAR ADC. This is the
  variant people actually bit‑bang SPI with.
- There is **no SPI peripheral exposed to the ULP on any ESP32**. ULP "SPI" is
  always GPIO bit‑bang, and Espressif provides **no official API/example** for
  it. The Arduino framework only exposes the weak ULP‑FSM, so the ULP‑RISC‑V path
  effectively means **ESP‑IDF**, not Arduino.

### 3.2 Every EPD control pin must be on an RTC‑capable GPIO

Only the RTC IO domain stays alive in deep sleep, so the ULP can only toggle
RTC GPIOs. The panel needs **CS, DC, RST, BUSY, SCK, MOSI** (and ideally the
panel power‑enable) all on RTC pins.

- **Classic ESP32** RTC GPIOs are a *sparse* set: 0, 2, 4, 12–15, 25–27, 32–39.
- **ESP32‑S3** RTC GPIOs are a *contiguous* range: **GPIO0–GPIO21** — much easier
  to satisfy.

> **Key finding:** the current Lilygo T5 v2.2 **cannot** do ULP e‑ink. Its SPI
> pins are GPIO5/18/19/23, none of which are RTC‑capable on classic ESP32. So
> this goal *requires* a board change no matter what.

### 3.3 SPI panel ≫ parallel panel for this goal

A SPI e‑paper (a handful of lines) is tractable to bit‑bang from the ULP. A
parallel/EPDiy panel (8 data lines + multiple clock/control lines, often routed
above GPIO21) is effectively impossible. **Prefer SPI panels.** This rules out
the big 4.7" parallel boards.

---

## 4. Board evaluation

### 4.1 LilyGo T5 E‑Paper S3 Pro — rejected (despite best PMIC)

- ESP32‑S3 (good ULP), and the **best power hardware** of anything surveyed:
  BQ25896 I²C charger + **BQ27220 fuel gauge** + TPS65185 EPD PMIC + PCF85063 RTC.
- **But** the panel is a 4.7" **ED047TC1 parallel** display (EPDiy, not GxEPD2),
  with control lines on GPIO41/42/45/48 — *outside* the RTC range and a parallel
  interface. **ULP‑hostile**, and it forces a full display rewrite.
- Verdict: gorgeous power management, wrong panel for the #1 requirement. **Skip.**

### 4.2 Waveshare ESP32‑S3‑ePaper‑1.54 / 1.54G — RECOMMENDED ✓

- ESP32‑S3 (ULP‑RISC‑V), **SPI** 1.54" 200×200 B/W panel.
- Onboard LiPo charging (ETA6098) + MP1605 3.3V buck, PCF85063 RTC, SHTC3
  temp/humidity (free win for a weather display).
- **Every EPD pin is verified inside GPIO0–21** (see §5). Even battery sense is on
  an RTC ADC pin, so the ULP could read battery voltage too.
- Only board where the make‑or‑break requirement is *provably* satisfied.

### 4.3 Waveshare ESP32‑S3‑ePaper‑3.97 — strong "bigger screen" alternative

- ESP32‑S3, **SPI** 3.97" **800×480** panel with 4‑gray + partial refresh
  (driver `EPD_3IN97_*`, confirmed SPI style).
- Better PMIC (**TG28**, configurable rails + charging), plus PCF85063 RTC, SHTC3,
  QMI8658 IMU, rotary encoder, much roomier dashboard.
- **Caveat:** Waveshare's 3.97" page omits the GPIO table; the EPD pin block
  (commonly 8–13 across these S3 boards) was **not** confirmable from a primary
  source. Verify the EPD GPIOs land in 0–21 against the board schematic before
  committing.

---

## 5. Pin verification (Waveshare ESP32‑S3‑ePaper‑1.54 / 1.54G)

From Waveshare's official GPIO table. **ESP32‑S3 RTC‑capable range = GPIO0–21.**

| EPD signal | GPIO | In RTC range? |
| --- | --- | --- |
| EPD_PWR (3V3 enable) | 6 | ✓ |
| EPD_BUSY | 8 | ✓ |
| EPD_RST | 9 | ✓ |
| EPD_DC | 10 | ✓ |
| EPD_CS | 11 | ✓ |
| EPD_SCLK | 12 | ✓ |
| EPD_SDI (MOSI) | 13 | ✓ |

All six control/SPI lines **and** the panel power‑enable are RTC GPIOs → the
ULP‑RISC‑V can both bit‑bang the panel and gate its rail during deep sleep.

Supporting pins:

| Function | GPIO | Notes |
| --- | --- | --- |
| BAT_ADC | 4 | ADC1 / RTC — readable from ULP (VBAT = VADC × 2) |
| RTC_INT (PCF85063) | 5 | RTC‑capable |
| BAT_KEY / BAT_Control | 18 / 17 | PWR latch circuit |
| RTC / SHTC3 I²C | SCL 48, SDA 47 | **Outside** RTC range — not ULP‑accessible |
| TF card (SDIO) | 39/40/41 | — |
| USB (native) | 19 (D‑), 20 (D+) | — |
| UART0 | TX 43, RX 44 | — |
| Charger | ETA6098 | single‑cell Li‑ion, charge‑only (no fuel gauge) |
| 3.3V DC‑DC | MP1605 | — |

> Gotcha: the external RTC and SHTC3 are on I²C GPIO47/48, *outside* the RTC
> domain, so the ULP can't read them mid‑sleep. That's fine — the ULP uses its
> own timer + the EPD pins above; the RTC/sensor are for the main core only.

---

## 6. Recommendation

**Primary: Waveshare ESP32‑S3‑ePaper‑1.54** (1.54G panel, or the Touch version if
you might want capacitive touch later).

- The one must‑pass box (SPI panel, all EPD pins on RTC GPIOs incl. power‑enable)
  is *provably* checked — uniquely so among surveyed boards.
- Onboard charging + RTC + battery ADC on an RTC pin.
- 200×200 ≈ 40k px — actually slightly more pixels than the current 296×128
  (~38k), just square; layout reworks but the GxEPD2 driver path stays.

**Step up only for the big screen:** ESP32‑S3‑ePaper‑3.97 — nicer PMIC and a far
roomier 800×480 dashboard, still SPI/4‑gray. Confirm its EPD GPIOs from the
schematic first.

**Avoid:** LilyGo T5 S3 Pro for this goal (parallel panel, ULP‑hostile) despite
its superior charger/fuel‑gauge.

---

## 7. Porting plan

Designed so the **T5 build stays intact** — the Waveshare board is added as a
second PlatformIO env, with board‑specific pins behind a `BOARD_WAVESHARE_S3_154`
build flag.

### 7.1 `platformio.ini` — add an S3 env

The shared `[env]` `lib_deps` are inherited; only the env below is new.

```ini
; Waveshare ESP32-S3-ePaper-1.54 (ESP32-S3-WROOM-1 N16R8, 200x200 SPI panel)
[env:s3_154]
platform = espressif32
board = esp32-s3-devkitc-1
board_build.mcu = esp32s3
board_build.flash_size = 16MB
board_build.arduino.memory_type = qio_opi   ; N16R8 = octal PSRAM
board_build.partitions = min_spiffs.csv
build_flags =
	-DDEBUG_BUILD
	-DBOARD_WAVESHARE_S3_154
	-DARDUINO_USB_CDC_ON_BOOT=1   ; S3 logs over native USB
	-DBOARD_HAS_PSRAM
```

`min_spiffs.csv` is a 4 MB layout; it works on the 16 MB part (leaves flash
unused). Swap to a 16 MB scheme later for more OTA/SPIFFS room.

### 7.2 `include/Config.h` — board‑specific pins

Wrap the existing T5 `pins` namespace; select the S3 map by the flag.

```cpp
namespace pins {
#if defined(BOARD_WAVESHARE_S3_154)
// Waveshare ESP32-S3-ePaper-1.54 — every EPD pin is in the RTC GPIO0–21 range.
constexpr int EPD_PWR = 6;    // panel 3V3 rail enable (drive HIGH to power)
constexpr int EPD_CS = 11;
constexpr int EPD_DC = 10;
constexpr int EPD_RST = 9;
constexpr int EPD_BUSY = 8;

constexpr int SPI_SCK = 12;
constexpr int SPI_MOSI = 13;
constexpr int SPI_MISO = -1;  // unused by the panel

constexpr int BAT_ADC = 4;    // RTC/ADC1 — readable from ULP too

// Wake buttons (all must be RTC GPIOs). BOOT has a pull-up already; add a
// button to the reserved GPIO1 header for the "enter setup" action.
constexpr gpio_num_t WAKE_REFRESH_GPIO = GPIO_NUM_0;  // BOOT button
constexpr gpio_num_t WAKE_SETUP_GPIO = GPIO_NUM_1;    // add your own button
#else
// Lilygo T5 v2.2 (classic ESP32)
constexpr int EPD_CS = 5;
constexpr int EPD_DC = 19;
constexpr int EPD_RST = 12;
constexpr int EPD_BUSY = 4;

constexpr int SPI_SCK = 18;
constexpr int SPI_MISO = 2;
constexpr int SPI_MOSI = 23;

constexpr uint8_t WAKE_TOUCH = T5;
constexpr uint8_t SETUP_TOUCH = T3;
constexpr int WAKE_TOUCH_THRESHOLD = 29;
constexpr int SETUP_TOUCH_THRESHOLD = 99;
#endif
}  // namespace pins
```

### 7.3 `src/display/Display.h` — conditional panel class

The 200×200 Waveshare 1.54" V2 is the SSD1681 `GxEPD2_154_D67`. **Confirm against
the controller marking on your panel** — the "G" sku may use a different LUT/class.

```cpp
#if defined(BOARD_WAVESHARE_S3_154)
  GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> gfx_{
      GxEPD2_154_D67(pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY)};
#else
  GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> gfx_{
      GxEPD2_290(pins::EPD_CS, pins::EPD_DC, pins::EPD_RST, pins::EPD_BUSY)};
#endif
```

### 7.4 `src/display/Display.cpp` — power rail + SPI remap in `begin()`

The S3 SPI pins aren't the defaults, so bind SPI (and power the panel) *before*
`init()`.

```cpp
void Display::begin() {
#if defined(BOARD_WAVESHARE_S3_154)
  pinMode(pins::EPD_PWR, OUTPUT);
  digitalWrite(pins::EPD_PWR, HIGH);                 // power the panel rail
  SPI.begin(pins::SPI_SCK, pins::SPI_MISO, pins::SPI_MOSI, pins::EPD_CS);
#endif
#ifdef DEBUG_BUILD
  gfx_.init(115200, false);
#else
  gfx_.init(0, false);
#endif
}
```

200×200 vs 296×128 means the column layout in `drawDayColumn` / `drawDaysAhead`
needs re‑proportioning — a separate cosmetic pass.

### 7.5 `src/power/SleepManager.cpp` — button wake instead of touch pads

Replace `enableTouchWake()` / `wakeupTouchGpio()` (classic‑ESP32 touch) with EXT1
wake using a two‑button bitmask (keeps the refresh‑vs‑setup distinction).

```cpp
#if defined(BOARD_WAVESHARE_S3_154)
void SleepManager::enableButtonWake() {
  const uint64_t mask =
      (1ULL << pins::WAKE_REFRESH_GPIO) | (1ULL << pins::WAKE_SETUP_GPIO);
  // S3 supports ANY_LOW; buttons are active-low to GND (ensure pull-ups).
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
}

int SleepManager::wakeupButtonGpio() {
  uint64_t status = esp_sleep_get_ext1_wakeup_status();
  if (status & (1ULL << pins::WAKE_SETUP_GPIO)) return pins::WAKE_SETUP_GPIO;
  if (status & (1ULL << pins::WAKE_REFRESH_GPIO)) return pins::WAKE_REFRESH_GPIO;
  return -1;
}
#endif
```

Then:

- The three sleep paths swap `enableTouchWake()` → `enableButtonWake()`.
- In `checkWakeReason`, the `ESP_SLEEP_WAKEUP_TOUCHPAD` branch becomes
  `ESP_SLEEP_WAKEUP_EXT1`, mapping `WAKE_SETUP_GPIO → EnterSettings`, else
  `RefreshNow`.
- Everything else — timer wake, drift compensation, `RTC_DATA_ATTR` state — is
  chip‑agnostic and carries over unchanged.
- Optional: after `display.hibernate()`, drive `EPD_PWR` LOW. E‑paper retains its
  image with the rail off, saving sleep current.

If a second button isn't wanted initially, drop to single‑button
`esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0)` and treat every press as
`RefreshNow`.

---

## 8. Touch power‑gating (Touch SKU only) — VERIFIED on schematic

Checked against `ESP32-S3-Touch-ePaper-1.54-Schematic.pdf`:

- The switchable **EPD3V3** rail is gated by **Q2 (AO3401)** via **EPD3V3_EN =
  GPIO6**, and feeds **only the e‑paper panel** (J10 VCI/VDD).
- The **FT6336 touch** block is powered from the **permanent 3V3 rail**, *not*
  EPD3V3. So driving `EPD_PWR`/GPIO6 LOW does **not** turn touch off.

**Implication:** if you buy the Touch SKU, the touch controller would stay
powered (and drain battery) in deep sleep unless you explicitly disable it.

**Fix (touch stays usable, but OFF in sleep):** `EPD_TP_RST = GPIO7` is
RTC‑capable and holds its level through deep sleep. Before sleeping, either:

- `digitalWrite(GPIO7 /*EPD_TP_RST*/, LOW)` to hold the FT6336 in reset (~µA), or
- command FT6336 hibernate over I²C (write `0x03` to register `0xA5`).

Wake/refresh via a button as planned. The non‑touch SKU (32298) avoids this
entirely and frees GPIO7/GPIO21.

## 9. Verify when the board arrives

- [ ] **Panel driver class** — `GxEPD2_154_D67` vs the "G" variant; confirm from
      the controller marking or Waveshare's example `#define`.
- [ ] **GPIO1 wake button** needs a pull-up (BOOT/GPIO0 already has one).
- [ ] **PSRAM memory type** — `qio_opi` for N16R8; adjust if a different module.
- [ ] **(Touch SKU)** add the `EPD_TP_RST`/GPIO7 reset‑hold to the sleep path.
- [ ] **(If choosing 3.97")** confirm EPD GPIOs are within 0–21 from the schematic.

---

## 10. Appendix — the ULP e‑ink path (reality check)

Even on the right board, "write the e‑ink from the ULP" is real work:

1. **Leave Arduino for ESP‑IDF** (or IDF‑as‑component under PlatformIO) — the
   ULP‑RISC‑V toolchain isn't available via the Arduino ULP‑FSM path.
2. **Hand‑write a bit‑banged SPI** in ULP‑RISC‑V C: toggle SCK/MOSI/CS via
   `RTC_GPIO_OUT_W1TS/W1TC`, read BUSY via `RTC_GPIO_IN`. No 50% duty clock is
   needed (SPI is clocked), but timing is a calibrated loop at ~17.5 MHz.
3. **RTC GPIO indices ≠ normal GPIO indices** — map through `rtc_io` tables.
4. **Pre‑stage the framebuffer + command sequence in RTC_SLOW_MEM** before sleep;
   the ULP streams it to the panel and re‑arms its timer.
5. The board choice only makes this *possible* (SPI panel + all pins RTC). It does
   not make it easy.

A pragmatic middle ground: keep the daily refresh on the main core (wake → WiFi →
fetch → render → sleep) and reserve the ULP for the cases that actually benefit
(e.g. a fast local partial‑refresh without a full core wake).

---

## 11. References

- Waveshare ESP32‑S3‑ePaper‑1.54 / 1.54G docs (GPIO table) —
  `docs.waveshare.com/ESP32-S3-ePaper-1.54` and `...-1.54G`
- Waveshare ESP32‑S3‑ePaper‑3.97 docs — `docs.waveshare.com/ESP32-S3-ePaper-3.97`
- LilyGo T5 E‑Paper S3 Pro wiki — `wiki.lilygo.cc` (BQ25896/BQ27220/TPS65185)
- ESP‑IDF ULP‑RISC‑V programming guide & sleep modes —
  `docs.espressif.com` (esp32s3)
- ESP‑IDF issue: ULP RISC‑V SPI support (IDFGH‑15605) — bit‑bang approach
- GxEPD2 (ZinggJM) — panel driver classes
