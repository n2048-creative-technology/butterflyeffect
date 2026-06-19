$fn=30;

// rod left:
translate([-13,0]) cylinder(d=1.5,h=30,center=true);

// rod right:
translate([13,0]) cylinder(d=1.5,h=30,center=true);

// motor shaft
rotate([90,0]) translate([0,0,4.1]) cylinder(d=3,h=10,center=true);

// plate
color("RED") rotate([90,0]) translate([0,0,.2]) cylinder(d=20,h=2,center=true);

// excentric pin
rotate([90,0]) rotate($t*1000) translate([8,0,-1]) cylinder(d=1.5,h=5,center=true);

// base
rotate([90,0]) translate ([0,0,3]) cube([30,31,2],center=true);
rotate([90,0]) translate ([0,0,3]) linear_extrude(2,center=true)  offset(4) offset(-4)square([55,10],center=true);

difference(){
    union(){
rotate([90,0]) translate ([0,14,0]) cube([30,3,5],center=true);
rotate([90,0]) translate ([0,-14,0]) cube([30,3,5],center=true);
}
translate([-13,0]) cylinder(d=1.6,h=40,center=true);
translate([13,0]) cylinder(d=1.6,h=40,center=true);
}


// wing pins
rotate([90,0]) translate([25,0,0]) cylinder(d=3,h=15,center=true);
rotate([90,0]) translate([-25,0,0]) cylinder(d=3,h=15,center=true);

// wings
rotate([90,0]) translate([25,0,-7]) rotate(-asin(8*sin($t*1000)/15)) linear_extrude(3) {
    hull(){
        circle(d=5);
        translate([-14,0]) circle(d=2);    
    };
    hull(){
        circle(d=2);
        translate([-18,0]) circle(d=2);    
    };
    hull(){
        circle(d=5);
        translate([10,10]) circle(d=5);    
    };
}

rotate([90,0]) translate([-25,0,-7]) rotate(asin(8*sin($t*1000)/15)) linear_extrude(3) {
    hull(){
        circle(d=5);
        translate([14,0]) circle(d=2);    
    };
    hull(){
        circle(d=2);
        translate([18,0]) circle(d=2);    
    };
    hull(){
        circle(d=5);
        translate([-10,10]) circle(d=5);    
    };
}


// car

translate([0,0,8*sin($t*1000)]) 
color("CYAN"){
translate([0,0,-2]) linear_extrude(2,center=true) {
    translate([-13,0]) difference() {    
        circle(d=3.6);
        circle(d=1.6);
    }
    translate([13,0]) difference() {    
        circle(d=3.6);
        circle(d=1.6);
    }
}

translate([0,0,2]) linear_extrude(2,center=true) {
    translate([-13,0]) difference() {    
        circle(d=3.6);
        circle(d=1.6);
    }
    translate([13,0]) difference() {    
        circle(d=3.6);
        circle(d=1.6);
    }
}

difference() {
    linear_extrude(8,center=true) {
        translate([0,2]) square([30,2],center=true);
    }
    linear_extrude(2,center=true) {
        translate([0,2]) square([21,3],center=true);
    }
}


// out pins 
rotate([90,0]) translate([12,2.5,-5]) cylinder(d=1.5,h=5,center=true);
rotate([90,0]) translate([12,-2.5,-5]) cylinder(d=1.5,h=5,center=true);
rotate([90,0]) translate([-12,2.5,-5]) cylinder(d=1.5,h=5,center=true);
rotate([90,0]) translate([-12,-2.5,-5]) cylinder(d=1.5,h=5,center=true);

}

