// K40 lid stay: holds the lid at the same angle every time it's opened, so a lid-mounted
// LightBurn camera comes back to the same spot. Mounts on the OUTSIDE of the case's left side,
// clear of the gantry and laser head.
//
// A slotted strut joins a bracket on the lid to a bracket on the case side. As the lid opens,
// the strut slides along the case bracket's spigot until the spigot hits the adjusting screw in
// the end of the slot. The stop is set just past vertical, so the lid's own weight holds it
// against the stop. The M3 adjusting screw tunes the angle (about 1 degree per mm).
//
// Side-view coordinates (all mm): origin = hinge pin axis, X forward along the closed lid,
// Y up. A = lid pivot, B = case pivot.
//
// Print in PETG, 0.2 mm layers, 4 walls, 30% infill, no supports. Orientations are as modelled.
// Hardware: 2 x M5x20 bolts + 2 x M5 nylock nuts + 2 x M5 fender washers (15-20 mm OD),
//           1 x M3x25 screw + 2 x M3 nuts (one captured, one as a lock nut),
//           4 x M4 bolts/nuts, 4 mm (5/32") rivets or #8 sheet-metal screws for mounting.

part = "assembly";   // "strut", "lid_bracket", "case_bracket", "parts" (print layout) or "assembly"
open_angle = 95;     // assembly view only: lid angle to show (0 = closed)

// --- Geometry: where the brackets go (measure from the hinge pin's centre) ---
a = 80;              // lid pivot: distance forward of the hinge axis, along the lid
e = 8;               // lid pivot: distance below the lid's top surface
b = 110;             // case pivot: distance forward of the hinge axis
c = 40;              // case pivot: distance below the hinge axis
stop_max = 100;      // lid angle with the adjusting screw fully backed out
stop_min = 90;       // lid angle with the screw fully in (keep it above 90 so gravity holds the lid)
lid_offset = 0;      // how far the lid's side face sits OUTSIDE the case's side face (negative = inside)

// --- Parts ---
$fn = 64;
strut_t = 8;   strut_w = 20;
spig_d  = 10;  spig_l = strut_t + 0.4;     // spigot the strut rides on
slot_w  = spig_d + 0.5;
boss_d  = 18;  boss_base = 6;
plate_t = 5;   hole_d = 4.5;               // mounting holes: M4, 4 mm rivet or #8 screw
bore    = 5.4;                             // M5
nut_af  = 8.2; nut_t = 5.4;                // M5 nylock pocket
m3      = 3.3; m3nut_af = 5.7; m3nut_t = 2.8;

h_lid  = boss_base + max(0, -lid_offset);
h_case = boss_base + max(0,  lid_offset);

function Apos(t) = [a*cos(t) + e*sin(t), a*sin(t) - e*cos(t)];
B = [b, -c];
function L(t) = norm(Apos(t) - B);
L0 = L(0); Lmax = L(stop_max); Lmin = L(stop_min);
slot_a = L0 - 8;                     // pin centre range in the slot (8 mm spare so the lid always closes)
slot_b = Lmax;
end_wall = 13;                       // past the slot: captured M3 nut and the screw head
strut_len = slot_b + slot_w/2 + end_wall;
echo(str("Strut ", round(strut_len), " mm. Pin travel ", round(L0), " (closed) to ", round(Lmax),
         " mm (", stop_max, " deg); screw in fully = ", round(Lmin), " mm (", stop_min, " deg)"));
assert(L(stop_max) > L(stop_max - 1) && L(1) > L(0), "Strut length must grow as the lid opens");
assert(Lmax - Lmin <= 10, "Adjusting range needs more than an M3x25 screw can reach");

// ------------------------------------------------------------------ strut
module strut() {
  difference() {
    hull() {
      cylinder(d = strut_w, h = strut_t);
      translate([strut_len - 1, -strut_w/2, 0]) cube([1, strut_w, strut_t]);
    }
    translate([0, 0, -1]) cylinder(d = slot_w, h = strut_t + 2);                     // pivot A
    hull() for (x = [slot_a, slot_b]) translate([x, 0, -1]) cylinder(d = slot_w, h = strut_t + 2);
    // adjusting screw along the strut's axis, M3 nut dropped in from the top
    translate([slot_b, 0, strut_t/2]) rotate([0, 90, 0]) cylinder(d = m3, h = strut_len, $fn = 24);
    translate([slot_b + slot_w/2 + 4, -m3nut_af/2, strut_t/2 - m3nut_af/2 - 0.2])
      cube([m3nut_t, m3nut_af, strut_t]);
    // engraved marks every 2 mm of screw travel (about 2 degrees), on the top face
    for (i = [0 : 2 : 10]) translate([slot_b + slot_w/2 - i, strut_w/2 - 2.5, strut_t - 0.6])
      cube([0.6, 3, 1]);
  }
}

// Spigot, boss and nut pocket. Local Z = outward from the mounting face; the boss starts at z0.
module pivot_add(z0, h) {
  translate([0, 0, z0]) cylinder(d = boss_d, h = h);
  translate([0, 0, z0 + h]) cylinder(d = spig_d, h = spig_l);
}
module pivot_cut(z0, h, nut_dir) {
  translate([0, 0, -1]) cylinder(d = bore, h = z0 + h + spig_l + 2, $fn = 32);
  // M5x20 + washer: the nut sits 4.6-10 mm below the boss top
  top = z0 + h;
  translate([0, 0, top - 10]) rotate([0, 0, nut_dir]) translate([0, -nut_af/2, 0])
    cube([boss_d, nut_af, nut_t]);
  translate([0, 0, top - 10]) cylinder(d = nut_af / cos(30), h = nut_t, $fn = 6);
}

// ------------------------------------------------------------------ case bracket
// Plate on the case's side wall. The pivot sits near the plate's top edge and both screws go
// below it, so nothing reaches up beside the lid's skirt. Prints plate-down.
case_top = boss_d/2 + 2;               // plate top edge above the pivot
module case_bracket() {
  w = 30; ht = case_top + 34;
  difference() {
    union() {
      translate([-w/2, -(ht - case_top), 0]) cube([w, ht, plate_t]);
      pivot_add(plate_t, h_case);
    }
    pivot_cut(plate_t, h_case, 0);
    for (x = [-8, 8]) translate([x, -24, -1]) cylinder(d = hole_d, h = plate_t + 2, $fn = 32);
  }
}

// ------------------------------------------------------------------ lid bracket
// L-shape: a tab screws to the lid's top near its side edge, a leg drops over the edge and carries
// the pivot. Modelled in print orientation: tab's top face on the bed, leg standing up, spigot
// pointing sideways (+Y). In the lid's frame: Z here = depth below the tab top, +Y = outward.
tab_t = 4; tab_w = 24; tab_in = 24; leg_t = plate_t;
leg_h = tab_t + e + boss_d/2 + 2;
module lid_bracket() {
  pz = tab_t + e;                                   // pivot height above the bed
  difference() {
    union() {
      translate([-tab_w/2, -tab_in, 0]) cube([tab_w, tab_in, tab_t]);              // tab
      translate([-tab_w/2, 0, 0]) cube([tab_w, leg_t, leg_h]);                     // leg
      // boss: round on top, flat to the bed so it prints without supports
      translate([0, leg_t, pz]) rotate([-90, 0, 0]) cylinder(d = boss_d, h = h_lid);
      translate([-boss_d/2, leg_t, 0]) cube([boss_d, h_lid, pz]);
      translate([0, leg_t + h_lid, pz]) rotate([-90, 0, 0]) cylinder(d = spig_d, h = spig_l);
    }
    translate([0, 0, pz]) rotate([-90, 0, 0]) rotate([0, 0, 90]) pivot_cut(leg_t, h_lid, 0);
    // nut drops in from the top
    translate([-nut_af/2, leg_t + h_lid - 10, pz]) cube([nut_af, nut_t, leg_h]);
    for (x = [-7, 7]) translate([x, -tab_in + 9, -1]) cylinder(d = hole_d, h = tab_t + 2, $fn = 32);
  }
}

// ------------------------------------------------------------------ views
// Side view: XY = the case's side plane, Z = outward from the case wall.
module lid_bracket_placed(t) {
  rotate([0, 0, t]) translate([a, 0, lid_offset])
    multmatrix([[1, 0, 0, 0], [0, 0, -1, tab_t], [0, 1, 0, 0]]) lid_bracket();
}
module side_view(t) {
  color("lightgrey") translate([-15, -190, -2]) cube([290, 190 - 4, 2]);               // case side
  color("steelblue", 0.7) rotate([0, 0, t]) translate([-4, -18, lid_offset - 60])
    cube([250, 18, 60]);                                                                 // lid
  color("dimgray") translate([0, 0, lid_offset]) cylinder(d = 8, h = 4);                 // hinge
  color("orange") translate([B[0], B[1], 0]) case_bracket();
  color("orange") lid_bracket_placed(t);
  P = Apos(t);
  ang = atan2(B[1] - P[1], B[0] - P[0]);
  color("gold") translate([P[0], P[1], plate_t + h_case]) rotate([0, 0, ang]) strut();
}

if (part == "strut") strut();
else if (part == "lid_bracket") lid_bracket();
else if (part == "case_bracket") case_bracket();
else if (part == "parts") {
  strut();
  translate([20, -40, 0]) case_bracket();
  translate([80, -45, 0]) lid_bracket();
}
else side_view(open_angle);
