# Atmo e-ink layout preview

A desktop harness that renders the **exact** on-device weather layout
(`src/display/weather_layout.h`) to PNGs, so the e-paper GUI can be designed and
iterated visually without flashing hardware.

It works because the layout is templated on the GFX object: the device uses
GxEPD2/Adafruit_GFX, and `host_gfx.h` reimplements the same drawing API and glyph
algorithm using the real Adafruit font data — so text is pixel-accurate to the
panel.

## Run

```bash
cd tools/preview
g++ -std=c++17 -O2 -I. -o /tmp/atmo_preview preview.cpp
/tmp/atmo_preview                      # writes /tmp/atmo_*.pgm
python3 pgm2png.py /tmp/atmo_t5_296x128.pgm /tmp/atmo_t5_296x128.png
```

`preview.cpp` renders the layout at several panel sizes (296x128, 200x200,
800x480) with sample data. Edit the sample `WeatherView` there to preview other
conditions/values.

## Files

- `host_gfx.h` — host reimplementation of the Adafruit GFX API (1-bit canvas).
- `Adafruit_GFX.h` — minimal stub so the real font headers compile on the host.
- `gfxfont.h`, `fonts/` — copies of the Adafruit GFXfont structs/data used both
  here and on the device.
- `pgm2png.py` — dependency-free PGM→PNG converter (scales up + adds a frame).

The single source of truth for the layout itself is
`src/display/weather_layout.h`; this harness only provides the host-side GFX
backend and a `main()`.
