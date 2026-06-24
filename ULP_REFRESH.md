# ULP-Driven Refresh — Plan & Honest Tradeoffs

> **Status: deferred (not implemented).** This documents *how* we'd drive the
> e-paper from the ULP coprocessor during deep sleep, and an honest accounting of
> why it isn't worth it for the current product. Read alongside `HARDWARE.md`.

## TL;DR

For Atmo as it exists (an **hourly clock + weather** that changes every hour),
ULP refresh would be a large rewrite for **~no battery benefit**, so we deliberately
did not build it. It only pays off if the screen shows **static content for long
stretches**. If that ever becomes the product, this is the plan.

---

## Why it's deferred — the honest tradeoffs

### 1. It doesn't remove the hourly CPU wake (the whole point) for changing content
The ULP would refresh the panel while the main cores sleep. But our content
changes every hour (the clock reads a new hour; the 24 h curve rolls). To make the
new frame the ULP would need to *generate* it, and it can't:
- **Can't read the time** — the PCF85063 RTC and the only I²C bus are on GPIO47/48,
  outside the RTC domain, so the ULP can't reach them mid-sleep.
- **Can't render** text/curves — that's far beyond a bit-bang blit.
- **Can't pre-store many frames** — one 200×200×1bpp frame is 5000 B and the S3's
  `RTC_SLOW_MEM` is only **8 KB**, shared with the ULP program itself. So at most
  one frame fits.

⇒ The CPU must still wake each hour to render the next frame, so the ULP saves
nothing. (Static content the ULP can simply repaint is the exception — see below.)

### 2. Our stack can't build it; it forces an ESP-IDF migration
- ULP-RISC-V requires the **ESP-IDF** build system. Arduino / PlatformIO-Arduino
  do **not** support it (our toolchain is arduino-esp32 2.0.17).
- There is **no ULP SPI peripheral or API** — Espressif issue **IDFGH-15605** is
  an open *feature request*. You hand-write **bit-banged SPI** in RISC-V.
- Migrating to IDF means re-homing WiFi, HTTPClient, GxEPD2, Preferences, and the
  captive-portal WebServer — effectively a ground-up rewrite of the app.

### 3. The power math is marginal even in the ideal case
- ULP draw is **~416 µA while running** vs **~34 µA** idle deep sleep (measured,
  `ddekanski/esp32_riscv_ulp_test`). A refresh keeps the ULP active for the panel's
  full ~1.3 s BUSY wait.
- The CPU wake it would replace is only ~0.008 mAh; the **daily energy is dominated
  by the once-a-day WiFi fetch**, not the brief hourly render.
- Net: low-single-digit % battery improvement, for a rewrite and ongoing fragility.

### When it *would* be worth it
- A mode that shows the **same image for hours/days** (e.g. a static dashboard,
  a photo, a name badge): the ULP can repaint or do nothing while the CPU stays
  asleep far longer — real savings.
- If Espressif ships first-class **ULP-SPI** + Arduino support (watch IDFGH-15605).

---

## Implementation plan (if/when pursued)

Phased, so each step is independently verifiable before the next.

### Phase 0 — Decide the framework
Move to **ESP-IDF** (or `arduino-as-component` under IDF if you want to keep the
Arduino APIs for the awake-path code). This is the gating commitment; everything
below depends on it. Track it as its own project, not a drop-in.

### Phase 1 — Prove ULP execution in deep sleep
Build the stock IDF example `system/ulp/ulp_riscv/gpio`: load with
`ulp_riscv_load_binary()` / `ulp_riscv_run()`, deep-sleep the CPU, confirm the ULP
toggles an RTC GPIO and can wake the CPU. Measure idle vs ULP-running current to
confirm the budget on *this* board.

### Phase 2 — Bit-bang SPI to the panel from the ULP
- Port a RISC-V soft-SPI (reference: `examples/peripherals/dedicated_gpio/soft_spi/.../soft_spi.S`),
  driving **SCLK=12, MOSI=13, CS=11, DC=10** via `RTC_GPIO_OUT_W1TS/W1TC` and
  reading **BUSY=8** via `RTC_GPIO_IN`. Note RTC-GPIO indices ≠ normal GPIO numbers
  (map via `rtc_io` tables). Keep the clock slow (≤ ~1 MHz) — RTC clock isn't a
  clean 50 % duty, but SPI is clocked so that's fine.
- Drive **EPD_PWR=6 LOW** (active-low) from the ULP to power the panel; raise it
  after to cut the rail.

### Phase 3 — SSD1681 sequence in the ULP
Replicate the `GxEPD2_154_D67` init + full-update sequence as a byte stream:
soft reset (0x12), driver/output control, write RAM (0x24, 5000 B from
`RTC_SLOW_MEM`), update control (0x22/0xF7), trigger (0x20), poll BUSY. Validate
against GxEPD2's command order in `.pio/libdeps/.../epd/GxEPD2_154_D67.cpp`.

### Phase 4 — Frame staging & wake/sleep integration
- Before sleep, the CPU renders the next frame into a 5000 B buffer in
  `RTC_SLOW_MEM` (mind the 8 KB total budget shared with the ULP program).
- ULP timer wakes hourly, blits the staged frame, re-arms its timer; CPU only
  wakes when it must produce a *new* frame (so this only helps if frames repeat).
- Keep VBAT latch (GPIO17) and pin holds from the current design.

### Phase 5 — Validate
Power-profile a full day vs the current main-core approach; confirm image quality
(no ghosting), and that the daily WiFi fetch + render still works on the CPU path.

---

## References
- ESP-IDF ULP-RISC-V guide (esp32s3): `docs.espressif.com/.../ulp-risc-v.html`
- ULP-SPI feature request: `github.com/espressif/esp-idf/issues/16235` (IDFGH-15605)
- ULP current measurements + Arduino/PlatformIO support gap: `github.com/ddekanski/esp32_riscv_ulp_test`
- Soft-SPI RISC-V reference: `esp-idf/examples/peripherals/dedicated_gpio/soft_spi`
- Panel command sequence: `GxEPD2` `epd/GxEPD2_154_D67.cpp`
