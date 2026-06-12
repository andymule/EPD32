# T5 2.9" + 1000 mAh battery case

A simple two-piece, no-hardware case for the **LilyGo T5 (ESP32, 2.9" b/w
e-paper)** with a ~1000 mAh LiPo on its back. Replaces the masking tape.

> All key dimensions below are **CONFIRMED against the real board** (June 2026).
> If you're adapting this for a different board, read "Lessons learned" first —
> it'll save you the half-dozen test prints it took to get here.

## Files

| File | What it is |
| --- | --- |
| `t5_case.scad` | Parametric source (edit numbers here) |
| `t5_case_plate.stl` | **Both parts on one plate — print this** |
| `t5_case_back.stl` | Back tray (battery + board drop in) |
| `t5_case_front.stl` | Front lid (screen window) |
| `t5_case_gauge.stl` | Spacing-finder comb (test tool, see below) |
| `t5_case_jig.stl` | Single 4-post coupon (fast post-fit test) |
| `preview_*.png` | Renders |

Outer size: **~98 × 48 × 13 mm**.

## How it works

Shoebox-lid fit with a **snap lock**: the front lid's skirt slides ~4 mm down
over the back tray (chamfered lead-in so it starts by hand), and a rounded bead
around the tray walls clicks into a groove inside the lid skirt — a positive
lock that holds shut in a bag but still pops apart by hand. The screen-window
bezel overhangs the front of the board on all four sides so it can't lift out,
and the board+battery sandwich is captured between the lid bezel and the tray
floor.

- **USB / charging end** — left fully open (two corner tabs retain the board).
- **Buttons** — a slot down one long side keeps all 3 exposed and pressable.
- **Power switch** — intentionally *not* exposed; sits under the wall.
- **Snap lock** — bead-and-groove detent rings the tray walls / lid skirt
  (`use_snap`); tune the click strength with `snap_proj`.
- **Locating posts** — 4 straight posts rise from the tray floor through the
  board's corner mounting holes and kill the side-to-side wiggle.
- **Headers** — assumes the unused GPIO header strip is **desoldered/clipped** so
  the LiPo lies flat (see Lessons learned #6).

## CONFIRMED board measurements

| Thing | Value | How we know |
| --- | --- | --- |
| Mounting-hole spacing, long (`mount_dx`) | **85.0 mm** center-to-center | gauge row "L85" |
| Mounting-hole spacing, short (`mount_dy`) | **32.5 mm** center-to-center | gauge row "S32.5" |
| Mounting-hole diameter (`hole_d`) | **3.0 mm** | measured |
| Sandwich thickness, desoldered (`stack_thick`) | **10.0 mm** | measured |
| Post diameter (`post_d`) | **2.6 mm** (= `hole_d − 0.4`) | clean slip-fit in the 3 mm holes |

Still nominal (board fit the cavity fine, never needed tightening):
`board_len = 89`, `board_w = 39.5`.

## Printing

- Print flat, **no supports**. PLA/PETG, 0.2 mm layers, 3 perimeters, ~15% infill.
- Re-export after editing the .scad:

```bash
openscad -o t5_case_plate.stl -D 'part="plate"' t5_case.scad
openscad -o t5_case_back.stl  -D 'part="back"'  t5_case.scad
openscad -o t5_case_front.stl -D 'part="front"' t5_case.scad
```

## Tuning (if a print is slightly off)

| Symptom | Fix |
| --- | --- |
| Board perches on posts / won't drop on | post spacing too wide → lower `mount_dx`/`mount_dy` |
| Board seated but wiggles | decrease `post_clear` (e.g. 0.4 → 0.3) for a tighter post |
| Lid too tight / too loose | `lid_gap` (bigger = looser) |
| Snap won't close / too hard | lower `snap_proj` (e.g. 0.7 → 0.5) |
| Snap pops open too easily | raise `snap_proj` (e.g. 0.7 → 0.9) |
| Buttons hard to reach | lengthen `btn_slot_len` / move `btn_from_bottom`; flip `btn_side` if mirrored |
| Sandwich doesn't fit / squished | raise `stack_thick` |

## Lessons learned (READ THIS before re-measuring anything)

These are the actual mistakes that cost test prints — don't repeat them.

1. **Measure hole spacing CENTER-to-CENTER, not edge-to-edge.** An edge-to-edge
   (outer-to-outer) reading over-reads by ~one hole diameter. A "83.5" caliper
   reading was really ~85 center-to-center once the 3 mm hole width was
   accounted for. The post centers are placed exactly at `mount_dx`/`mount_dy`.

2. **Don't trust by-eye "it's off by X mm" estimates.** They were unreliable and
   even sign-flipped (a too-*short* gap got reported as too long). When a fit is
   ambiguous, use the gauge:

   ```bash
   openscad -o t5_case_gauge.stl -D 'part="gauge"' t5_case.scad
   ```

   Print it, drop the board's two long-edge holes onto each wide post pair and
   the two short-edge holes onto each narrow pair. The row that seats flush is
   the real spacing. `L##` rows = long-edge candidates, `S##` = short-edge.
   (Note: the embossed labels render rough at small size — an "8" can look like a
   "3". Trust which physical row fits, not the glyph.)

3. **Locating posts must be STRAIGHT shanks, not cones.** A tapered post only
   lets the hole clear at the skinny tip, so the board perches on the cone and
   sits proud. Use a straight shank (`post_d`) sized to the hole, with only a
   tiny pointed lead-in at the very top.

4. **Four posts over-constrain.** With four holes, the spacing must match in both
   axes simultaneously. Leave ~0.4 mm diametral clearance (`post_clear`) so a
   few tenths of error doesn't bind across all four.

5. **The battery lifts the board off the tray floor (~6 mm).** The board's
   corners float above the floor, so posts must be tall enough (`post_h = 8`) to
   bridge that gap *and* still engage the holes. Without the battery in, the
   board drops further and the posts stick up proud — that's expected.

6. **Desolder the unused GPIO header strip.** Those breakout pins (the Atmo
   firmware uses none of them) tent the LiPo and, worse, a pouch cell pressed on
   bare pin tips is a puncture/fire risk. Clipping them flat keeps the case slim
   (~13 mm) and safe. If you must keep them, raise `stack_thick` and add a rigid
   shelf over the tips.

7. **Orientation:** screen toward you, USB at the bottom → buttons on the right
   (`btn_side = "right"`). Flip to `"left"` if your openings come out mirrored.
