// K40 magnetic lid stop. A printed wedge on the top panel just behind the lid's hinge: the lid's
// outer face leans back flat against it, and two magnets in the face hold the steel lid there.
// Opened this way the lid lands at the same angle every time, which keeps a lid-mounted LightBurn
// camera calibrated. Nothing is inside the cutting area. Use two, one beside each hinge.
//
// The face is angled to match the lid at `stop_angle` (95 = 5 degrees past vertical, so the lid's
// weight keeps it against the stop). Sliding the block forward or back in its slots fine-tunes the
// angle (about 1.2 degrees per mm), so nothing needs measuring.
//
// Print base-down in PETG, 0.2 mm layers, 4 walls, 25% infill, no supports.
// Per stop: 2 x 10x3 mm disc magnets (N52, glued in with CA), and either 3M VHB tape (5952) under
// the base or 2 x M4 screws through the slots.

stop_angle = 95;     // lid angle the face matches (0 = closed, 90 = vertical)
H = 55;              // block height above the panel
W = 40;              // block width, along the hinge line
D = 26;              // block depth at the top
relief = 18;         // contact face starts this high, leaving room for the hinge knuckle
relief_back = 7;     // and is cut back this far below that
base_t = 4;
tail = 24;           // base behind the block, carrying the screw slots
slot_len = 16;       // adjustment travel
mag_d = 10.3; mag_t = 3.0; mag_recess = 0.4;   // 10x3 magnets sit 0.4 mm below the face
mag_z = [32, 47];

$fn = 64;
lean = stop_angle - 90;                        // face leans back this many degrees
function xf(z) = -z * tan(lean);               // front face x at height z (front = +X)

module profile() {
  polygon([
    [-D - tail, 0], [-relief_back, 0], [-relief_back, relief - relief_back],
    [xf(relief), relief], [xf(H), H], [xf(H) - D, H], [xf(H) - D, base_t], [-D - tail, base_t]
  ]);
}

module lid_stop() {
  difference() {
    rotate([90, 0, 0]) translate([0, 0, -W/2]) linear_extrude(W) profile();
    // magnet pockets, square to the face
    for (z = mag_z) translate([xf(z), 0, z]) rotate([0, -90 + lean, 0])
      translate([0, 0, -0.01]) cylinder(d = mag_d, h = mag_t + mag_recess);
    // screw slots in the tail (M4, or #8 screws)
    for (y = [-W/2 + 9, W/2 - 9]) hull() for (dx = [0, slot_len - 4.5])
      translate([-D - tail + 4 + 2.25 + dx + (tail - slot_len - 4) / 2 + 2, y, -1])
        cylinder(d = 4.5, h = base_t + 2, $fn = 32);
    // label
    translate([xf(H) - D/2, 0, H - 0.6]) linear_extrude(1)
      text(str(stop_angle, "°"), size = 6, halign = "center", valign = "center");
  }
}

// Side-view check: the lid resting on the stop at stop_angle (blue), hinge pin at the origin.
module assembly() {
  color("orange") lid_stop();
  color("lightgrey") translate([-90, -60, -3]) cube([90, 120, 3]);                      // panel
  color("steelblue", 0.6) rotate([0, -stop_angle, 0]) translate([0, -60, 0]) cube([220, 120, 1.2]); // lid
}

part = "stop";   // "stop" or "assembly"
if (part == "assembly") assembly(); else lid_stop();
