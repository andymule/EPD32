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
/tmp/atmo_preview                      # writes /tmp/atmo_<style>_<size>.pgm
python3 pgm2png.py /tmp/atmo_horizon_portrait_128x296.pgm out.png
```

`preview.cpp` renders **both layouts** (`horizon` and `dashboard`) at the panel
sizes we target (296x128 landscape, 200x200 square, 128x296 portrait) with sample
data, so the outputs are named `/tmp/atmo_<style>_<size>.pgm`. Edit the sample
`WeatherView` in `preview.cpp` to preview other conditions/values.

## Files

- `host_gfx.h` — host reimplementation of the Adafruit GFX API (1-bit canvas).
- `Adafruit_GFX.h` — minimal stub so the real font headers compile on the host.
- `gfxfont.h`, `fonts/` — copies of the Adafruit GFXfont structs/data used both
  here and on the device.
- `pgm2png.py` — dependency-free PGM→PNG converter (scales up + adds a frame).

The single source of truth for the layout itself is
`src/display/weather_layout.h`; this harness only provides the host-side GFX
backend and a `main()`.
