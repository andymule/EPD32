// =============================================================================
// Atmo  -  LilyGo T5 2.9" e-paper + 1000 mAh battery "sandwich" case
// =============================================================================
// A simple two-piece, no-hardware case (shoebox-lid friction fit) that clamps
// the board and the battery taped to its back into one tidy package, so you can
// ditch the masking tape.
//
//   * Back shell  : shallow tray, the battery + board drop in.
//   * Front shell : lid with the screen window; its skirt slides down OVER the
//                   back shell and its inner ceiling presses on the glass, so
//                   the whole sandwich is clamped between lid ceiling and tray
//                   floor. Friction fit, pops apart by hand, no screws/glue.
//   * Bottom (USB) end is left open  -> charging port always reachable.
//   * One long side has a slot       -> the 3 buttons stay exposed/pressable.
//   * The slide power switch is intentionally NOT exposed (you said you don't
//     need it); it sits under the wall on the non-button side.
//
// Print both parts flat on the bed, no supports. PLA/PETG, 0.2 mm layers.
// All dimensions in millimetres.
// =============================================================================

part = "both";   // "back"|"front"|"both"|"exploded"|"plate"|"demo"|"section"|"jig"

// ----------------------------- MEASURE THESE --------------------------------
board_len   = 89.0;   // PCB long dimension (USB end -> top of board)
board_w     = 39.5;   // PCB width (matches the battery width, per your note)
stack_thick = 10.0;   // TOTAL thickness of the taped board+battery sandwich
                      // (front glass face all the way to the back of battery)

// Screen window: sized to the ACTIVE display (~66.9 x 29.0 mm), NOT the full
// glass. This leaves a solid bezel/lip overhanging the board front on all four
// sides (that lip is what traps the board in), and keeps the e-ink ribbon -
// which sits just past the active area toward the bottom - covered.
win_len      = 71.0;  // window length (down the board): +4 vs active so the
                      // bottom edge drops 6 mm total (top drops 2 via win_top_gap)
win_w        = 30.0;  // window width (across the board) ~ active area
win_top_gap  = 6.5;   // gap from the TOP edge of the board to the window (+2 mm)
                      // (covers the glass top margin; bottom margin is larger
                      //  automatically, which hides the ribbon)

// Button slot (runs along ONE long side near the USB end).
btn_side        = "right"; // "right" or "left" (screen toward you, USB at bottom)
btn_from_bottom = 2.0;     // slot start, measured up from the USB end
btn_slot_len    = 67.0;    // how far the slot runs up the side (+7 mm of reach
                           // for the farthest-from-USB button)

// USB-end opening (kept open for charging). Corner posts retain the board.
usb_corner_post = 5.0;     // width of the corner retainers at the USB end

// Locating posts up through the board's 4 corner mounting holes. These pin the
// board's X-Y position (kills the wiggle) and add retention. The board floats
// on top of the battery, so the posts are tall enough to bridge that gap and
// still engage the holes.
//   >>> MEASURE these two: hole CENTER-to-CENTER spacing on your board <<<
use_posts   = true;
mount_dx    = 85.0;   // hole spacing along the LENGTH (USB end <-> top)  [gauge: L85]
mount_dy    = 32.5;   // hole spacing across the WIDTH                    [gauge: S32.5]
hole_d      = 3.0;    // board mounting-hole diameter                     [measured]
post_clear  = 0.4;    // diametral slip-fit clearance (bigger = more forgiving)
post_d      = hole_d - post_clear;  // 2.6 mm STRAIGHT shank through the 3 mm hole
post_lead   = 1.4;    // short pointed lead-in at the tip (self-guides into the hole)
post_h      = 8.0;    // post height from the tray floor

// ------------------------------ FIT / SHELL ---------------------------------
clr        = 1.0;   // clearance around the board inside the cavity (per side)
z_clr      = 0.5;   // vertical slack on the stack thickness (keeps rattle low)
wall       = 1.6;   // shell wall thickness
back_floor = 1.4;   // thickness of the back (battery) floor
front_face = 1.4;   // thickness of the front face (around the window)
lid_gap    = 0.25;  // slip-fit gap between lid skirt and back shell (firm fit)
mouth_lead = 0.9;   // chamfered lead-in at the lid mouth so it starts by hand
corner_r   = 3.0;   // tray outer corner rounding

// How the stack is split between the two shells, sized for a deep, firm grip.
back_frac  = 0.60;  // back tray wall height as a fraction of interior height
lid_frac   = 0.80;  // lid skirt length as a fraction of interior height
                    // overlap = (back_frac + lid_frac - 1) * interior_h

// ----------------------------------------------------------------------------
$fn = 64;
eps = 0.02;

// Interior the sandwich lives in
int_h   = stack_thick + z_clr;          // floor-inner to lid-ceiling-inner
cav_w   = board_w + 2*clr;
cav_l   = board_len + 2*clr;

// Back shell
back_wall_h = back_frac * int_h;
back_ow     = cav_w + 2*wall;
back_ol     = cav_l + 2*wall;
back_h      = back_floor + back_wall_h;  // total printed height of back shell

// Front shell (lid) - skirt wraps OVER the back shell outer wall.
// Corner radii are chained so the skirt is constant thickness AND its inner
// corners actually mate with the tray's rounded outer corners (tray_r + gap).
lid_skirt = lid_frac * int_h;
skirt_t   = wall;
lid_in_r  = corner_r + lid_gap;          // skirt inner corner (mates tray outer)
lid_out_r = corner_r + lid_gap + skirt_t;// skirt outer corner
front_ow  = back_ow + 2*(lid_gap + skirt_t);
front_ol  = back_ol + 2*(lid_gap + skirt_t);
front_h   = front_face + lid_skirt;      // total printed height of lid

overlap   = back_wall_h + lid_skirt - int_h;
echo(str("Assembled thickness ~= ", back_floor + int_h + front_face, " mm; ",
         "lid/back overlap ~= ", overlap, " mm"));

// ---- snap-lock (bead on the tray, groove in the lid) ------------------------
use_snap   = true;
snap_proj  = 0.7;     // how far the tray bead sticks out past the wall
snap_clear = 0.2;     // groove radial clearance over the bead
snap_tol   = 0.4;     // extra groove height (tolerates seat-depth variation)
snap_inset = corner_r + 3;                       // keep beads off the corners
lid_seat_z = back_floor + int_h - lid_skirt;     // lid Z when fully closed
snap_zc    = (lid_seat_z + back_h) / 2;          // bead height (global, mid-overlap)
snap_z_lid = snap_zc - lid_seat_z;               // same height in lid-local coords

// ---- helpers ---------------------------------------------------------------
module rrect(l, w, h, r) {
    linear_extrude(height=h)
        offset(r=r) offset(r=-r)
            square([l, w], center=true);
}

// Button slot solid (x runs along length, USB end at x = -cav_l/2)
module button_slot_solid() {
    side = (btn_side == "right") ? 1 : -1;
    translate([btn_from_bottom + btn_slot_len/2 - cav_l/2,
               side * (cav_w/2 + wall), -5])
        cube([btn_slot_len, wall*4, int_h + 20], center=true);
}

// USB-end opening: open the whole bottom end except small corner posts
module usb_open_solid() {
    open_w = cav_w - 2*usb_corner_post;
    translate([-cav_l/2 - wall, 0, -5])
        cube([wall*4, open_w, int_h + 20], center=true);
}

// Locating posts: straight slip-fit shank + short pointed lead-in tip.
// (Straight so the board hole clears it for the full depth, not just the tip.)
module locating_posts() {
    shank = post_h - post_lead;
    for (sx = [-1, 1], sy = [-1, 1])
        translate([sx*mount_dx/2, sy*mount_dy/2, back_floor - eps]) {
            cylinder(h = shank, d = post_d);
            translate([0, 0, shank - eps])
                cylinder(h = post_lead, d1 = post_d, d2 = post_d - 1.2);
        }
}

// A single gauge post (straight slip-fit shank + short lead-in tip)
module gpost(h) {
    lead = 1.0;
    cylinder(h = h - lead, d = post_d);
    translate([0, 0, h - lead - eps])
        cylinder(h = lead, d1 = post_d, d2 = post_d - 1.0);
}

// Snap-lock bead that rings the tray's outer wall (long walls + the closed
// top wall). The USB/button cutouts trim it automatically.
module tray_beads() {
    for (sy = [-1, 1])
        translate([0, sy*back_ow/2, snap_zc]) rotate([0, 90, 0])
            cylinder(h = back_ol - 2*snap_inset, r = snap_proj, center = true);
    translate([back_ol/2, 0, snap_zc]) rotate([90, 0, 0])
        cylinder(h = back_ow - 2*snap_inset, r = snap_proj, center = true);
}

// Matching groove inside the lid skirt (a touch taller than the bead for
// seat-depth tolerance).
module lid_grooves() {
    gr = snap_proj + snap_clear;
    for (sy = [-1, 1])
        translate([0, sy*(back_ow/2 + lid_gap), 0])
            hull() for (dz = [-snap_tol, snap_tol])
                translate([0, 0, snap_z_lid + dz]) rotate([0, 90, 0])
                    cylinder(h = front_ol, r = gr, center = true);
    translate([back_ol/2 + lid_gap, 0, 0])
        hull() for (dz = [-snap_tol, snap_tol])
            translate([0, 0, snap_z_lid + dz]) rotate([90, 0, 0])
                cylinder(h = front_ow, r = gr, center = true);
}

// ---- BACK SHELL ------------------------------------------------------------
module back_shell() {
    union() {
        difference() {
            union() {
                rrect(back_ol, back_ow, back_h, corner_r);
                if (use_snap) tray_beads();
            }
            translate([0,0,back_floor])
                rrect(cav_l, cav_w, back_h, max(0.1, corner_r-wall));
            usb_open_solid();
            button_slot_solid();
        }
        if (use_posts) locating_posts();
    }
}

// ---- FRONT SHELL (lid) -----------------------------------------------------
module front_shell() {
    difference() {
        union() {
            // top face plate
            translate([0,0, lid_skirt])
                rrect(front_ol, front_ow, front_face, lid_out_r);
            // skirt
            difference() {
                rrect(front_ol, front_ow, lid_skirt, lid_out_r);
                // cavity the back shell slides into
                translate([0,0,-eps])
                    rrect(back_ol + 2*lid_gap, back_ow + 2*lid_gap,
                          lid_skirt + 2*eps, lid_in_r);
                // chamfered lead-in at the mouth (z = 0) for easy assembly
                hull() {
                    translate([0,0,-eps])
                        rrect(back_ol + 2*lid_gap + 2*mouth_lead,
                              back_ow + 2*lid_gap + 2*mouth_lead,
                              eps, lid_in_r + mouth_lead);
                    translate([0,0, mouth_lead])
                        rrect(back_ol + 2*lid_gap, back_ow + 2*lid_gap,
                              eps, lid_in_r);
                }
            }
        }
        // screen window cut FULLY through the top face (window near the top edge).
        // Centered on the mid-plane of the face plate so it pierces top to bottom.
        win_cx = cav_l/2 - win_top_gap - win_len/2;
        translate([win_cx, 0, lid_skirt + front_face/2])
            cube([win_len, win_w, front_face + 6*eps], center=true);
        // USB + button openings also cut the lid
        usb_open_solid();
        button_slot_solid();
        // snap-lock groove
        if (use_snap) lid_grooves();
    }
}

// ---- GHOST BOARD + BATTERY (for preview only) ------------------------------
// A simple stand-in for the real board+battery sandwich, so you can see how the
// lip traps it. Sits on the tray floor, fills the cavity.
module mock_stack() {
    board_t = 1.6;
    glass_t = 1.2;
    bat_t   = 6.0;                 // realistic LiPo; board floats on top of it
    z0 = back_floor;
    // battery (central, leaves the corner ears free for the posts)
    color("DimGray")
        translate([0,0,z0])
            rrect(56, 34, bat_t, 2);
    // pcb (floats on the battery), with holes for the locating posts
    color("ForestGreen")
        translate([0,0,z0+bat_t])
            difference() {
                rrect(board_len, board_w, board_t, 1);
                if (use_posts)
                    for (sx=[-1,1], sy=[-1,1])
                        translate([sx*mount_dx/2, sy*mount_dy/2, -1])
                            cylinder(h=board_t+2, d=hole_d);
            }
    // e-paper glass on front (only over the screen region near the top)
    color("WhiteSmoke")
        translate([cav_l/2 - win_top_gap - 79/2 + 1, 0, z0+bat_t+board_t])
            cube([79, 36.7, glass_t], center=true);
}

// ---- LAYOUT ----------------------------------------------------------------
if (part == "back") back_shell();
else if (part == "front") translate([0,0,front_h]) rotate([180,0,0]) front_shell();
else if (part == "exploded") {
    back_shell();
    translate([0,0, back_h + 20]) front_shell();
} else if (part == "jig") {                   // FAST post-spacing test coupon
    // Thin baseplate + the 4 locating posts only. Prints in a couple minutes.
    // One corner is clipped flat to mark orientation (the clipped corner = the
    // USB end / -X,-Y corner of the board).
    jig_l = mount_dx + 14;
    jig_w = mount_dy + 14;
    difference() {
        rrect(jig_l, jig_w, back_floor, 3);
        translate([-jig_l/2, -jig_w/2, -1])
            cube([8, 8, back_floor + 2]);
    }
    locating_posts();
} else if (part == "gauge") {                 // SPACING FINDER comb (one print)
    // Post PAIRS at a range of spacings, each labelled. Drop your board's two
    // holes onto each pair; the one that seats perfectly is your real spacing.
    //   L## rows  = LONG-edge spacing candidates (use 2 holes on a long edge)
    //   S## rows  = SHORT-edge spacing candidates (use 2 holes on a short edge)
    gp_t   = 1.6;            // plate thickness
    gh     = 5;             // gauge post height (short = fast print)
    longs  = [80, 81, 82, 83, 84, 85, 86];
    shorts = [31.5, 32, 32.5];
    pitch  = 11;
    labw   = 13;            // left strip for the printed labels
    maxs   = 86;
    cx     = labw + 6 + maxs/2;
    plate_w = labw + 6 + maxs + 6;
    n       = len(longs) + len(shorts);
    plate_l = n*pitch + 6;

    cube([plate_w, plate_l, gp_t]);            // baseplate (corner at origin)
    for (i = [0:len(longs)-1]) {
        s = longs[i];  y = 3 + i*pitch + pitch/2;
        for (sx = [-1,1]) translate([cx + sx*s/2, y, gp_t-eps]) gpost(gh);
        translate([2, y-3.2, gp_t-eps]) linear_extrude(0.6) text(str("L",s), size=4.2);
    }
    for (j = [0:len(shorts)-1]) {
        s = shorts[j];  y = 3 + (len(longs)+j)*pitch + pitch/2;
        for (sx = [-1,1]) translate([cx + sx*s/2, y, gp_t-eps]) gpost(gh);
        translate([2, y-3.2, gp_t-eps]) linear_extrude(0.6) text(str("S",s), size=4.0);
    }
} else if (part == "xsec") {                  // width slice to show the snap
    intersection() {
        union() {
            color("Orange", 0.95) back_shell();
            translate([0,0, lid_seat_z]) color("DodgerBlue", 0.95) front_shell();
        }
        translate([30, 0, 0]) cube([6, 300, 300], center=true);
    }
} else if (part == "plate") {                 // both parts laid out to print
    back_shell();
    translate([0, back_ow + 8, front_h]) rotate([180,0,0]) front_shell();
} else if (part == "demo") {                  // assembled + ghost board inside
    color("Orange", 0.9) back_shell();
    mock_stack();
    translate([0,0, back_floor + int_h - lid_skirt])
        color("DodgerBlue", 0.40) front_shell();
} else if (part == "section") {               // cut-away showing the top lip
    difference() {
        union() {
            color("Orange", 0.95) back_shell();
            mock_stack();
            translate([0,0, back_floor + int_h - lid_skirt])
                color("DodgerBlue", 0.95) front_shell();
        }
        // slice away the +y half to expose the cross-section
        translate([0, back_ow, -10])
            cube([back_ol*2, back_ow*2, int_h*4], center=true);
    }
} else {                                       // assembled preview
    back_shell();
    translate([0,0, back_floor + int_h - lid_skirt])
        color("DodgerBlue", 0.55) front_shell();
}
