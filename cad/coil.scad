$fn = 100;
durchmesser=30;
rille=8;
wand=1;

module torus(d1, d2) {
    rotate_extrude(convexity = 10)
    translate([d2/2+d1/2, 0, 0])
    circle(r = d1/2);
}

intersection() {
    difference() {
        torus(rille+wand*2,durchmesser-wand*2);
        torus(rille,durchmesser);
    }
    translate([0,0,-5]) cylinder(h=10, r=(durchmesser+rille)/2);
}
