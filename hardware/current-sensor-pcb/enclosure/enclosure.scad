// Enclosure for the K40 tube current sensor PCB (68 x 42 mm, 4 x M3 at 3.5 mm from the edges).
// Two parts: base (PCB on standoffs, wire slots in the end walls) and lid (2 x M3 screws).
// Print in PETG or ABS, 0.2 mm layers, 3 walls, no supports.
//   base: as modelled (floor down)      lid: flip it, top face on the bed
// Screws: 4 x M3x6 for the PCB, 2 x M3x8 for the lid, self-tapping into the printed holes
// (or use M3 heat-set inserts: set INSERTS = true).

part = "both";          // "base", "lid" or "both" (both = layout preview)
INSERTS = false;        // true = holes sized for M3 x 4 mm heat-set inserts

$fn = 48;
pcb  = [68, 42, 1.6];
hole = 3.5;             // PCB hole inset
gap_x = 2;              // PCB to wall, terminal ends
gap_y = 6;              // PCB to wall, long sides (room for the lid screw posts)
wall = 2.2;
floor_t = 2;
stand_h = 5;            // PCB underside above the floor (clears soldered pins)
term_h = 11;            // tallest terminal above the PCB, plus a little
lid_t = 2;
lip = 2.5; lip_gap = 0.25;
pilot = INSERTS ? 4.2 : 2.6;

in   = [pcb[0] + 2*gap_x, pcb[1] + 2*gap_y];
in_h = stand_h + pcb[2] + term_h;
out  = [in[0] + 2*wall, in[1] + 2*wall];
base_h = floor_t + in_h;
pcb_z = floor_t + stand_h;           // PCB underside
top_z = pcb_z + pcb[2];              // PCB top
function P(x, y) = [wall + gap_x + x, wall + gap_y + y];   // PCB coords -> box coords

// terminal wire openings (PCB coords, y ranges) and their height above the PCB top
slot_l = [6, 35, 9];     // J1 (y 9.5-20) + J2 (y 27.5-31), panel side
slot_r = [14, 36, 10];   // J3 (y 19.8-30), tube side
// Lid screws sit beside the PCB's long edges, one over each side of the isolation barrier and
// well away from it, so a metal screw never bridges the two halves.
lid_posts = [[wall + gap_x + 14, wall + 2.6], [wall + gap_x + 54, out[1] - wall - 2.6]];

module rbox(s, r=2) { linear_extrude(s[2]) offset(r) offset(-r) square([s[0], s[1]]); }

module base() {
    difference() {
        union() {
            difference() {
                rbox([out[0], out[1], base_h], 3);
                translate([wall, wall, floor_t]) cube([in[0], in[1], base_h]);
            }
            // PCB standoffs
            for (x = [hole, pcb[0]-hole], y = [hole, pcb[1]-hole])
                translate([each P(x, y), 0]) cylinder(d=6.5, h=pcb_z);
            // lid screw posts (merged into the long walls)
            for (p = lid_posts) translate([p[0], p[1], 0]) cylinder(d=6, h=base_h);
            // mounting ears on the ends
            for (x = [-9, out[0]]) translate([x, out[1]/2 - 8, 0])
                linear_extrude(3) offset(2) offset(-2) square([9 + 2, 16]);
        }
        for (x = [hole, pcb[0]-hole], y = [hole, pcb[1]-hole])
            translate([each P(x, y), floor_t]) cylinder(d=pilot, h=pcb_z);
        for (p = lid_posts) translate([p[0], p[1], base_h - 8]) cylinder(d=pilot, h=9);
        for (x = [-4.5, out[0] + 4.5]) translate([x, out[1]/2, -1]) cylinder(d=4.3, h=5);
        // wire slots
        translate([-1, wall + gap_y + slot_l[0], top_z - 0.5]) cube([wall + 2, slot_l[1] - slot_l[0], slot_l[2] + 0.5]);
        translate([out[0] - wall - 1, wall + gap_y + slot_r[0], top_z - 0.5]) cube([wall + 2, slot_r[1] - slot_r[0], slot_r[2] + 0.5]);
        // side labels
        translate([0.6, out[1]/2, 3.2]) rotate([90, 0, -90]) linear_extrude(1)
            text("PANEL", size=3.4, halign="center", font="Liberation Sans:style=Bold");
        translate([out[0] - 0.6, out[1]/2, 3.2]) rotate([90, 0, 90]) linear_extrude(1)
            text("TUBE", size=3.4, halign="center", font="Liberation Sans:style=Bold");
    }
}

module lid() {
    difference() {
        union() {
            rbox([out[0], out[1], lid_t], 3);
            // lip that sits inside the walls (clears the screw posts and wire slots)
            translate([0, 0, -lip]) difference() {
                translate([wall + lip_gap, wall + lip_gap, 0]) cube([in[0] - 2*lip_gap, in[1] - 2*lip_gap, lip]);
                translate([wall + lip_gap + 1.6, wall + lip_gap + 1.6, -1])
                    cube([in[0] - 2*lip_gap - 3.2, in[1] - 2*lip_gap - 3.2, lip + 2]);
                for (p = lid_posts) translate([p[0], p[1], -1]) cylinder(d=6.6, h=lip + 2);
            }
        }
        for (p = lid_posts) translate([p[0], p[1], -lip - 1]) cylinder(d=3.4, h=lid_t + lip + 2);
        for (p = lid_posts) translate([p[0], p[1], lid_t - 1.6]) cylinder(d1=3.4, d2=6.6, h=1.61);
        // top text, debossed 0.6 mm
        translate([0, 0, lid_t - 0.6]) linear_extrude(1) {
            translate([out[0]/2, out[1]/2 + 6]) text("K40 TUBE mA SENSOR", size=4.2, halign="center", valign="center", font="Liberation Sans:style=Bold");
            translate([out[0]/2, out[1]/2 - 2]) text("isolated  -  2.5 kV", size=3, halign="center", valign="center", font="Liberation Sans");
            translate([wall + 3, out[1]/2 - 12]) text("< I2C / 5V", size=3, valign="center", font="Liberation Sans");
            translate([out[0] - wall - 3, out[1]/2 - 12]) text("IN OUT EARTH >", size=3, halign="right", valign="center", font="Liberation Sans");
        }
    }
}

if (part == "none") {} else if (part == "base") base();
else if (part == "lid") translate([0, 0, lid_t]) rotate([180, 0, 0]) translate([0, -out[1], 0]) lid();
else { base(); translate([0, out[1] + 12, lid_t]) rotate([180, 0, 0]) translate([0, -out[1], 0]) lid(); }

echo(str("outside: ", out[0], " x ", out[1], " x ", base_h + lid_t, " mm (without ears)"));
