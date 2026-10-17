// Laser tube current (mA) from an INA226 or INA219 on the panel's I2C-OUT header,
// placed (through an I2C isolator) in the tube's cathode return wire. See docs/tube-current-sensor.md.
#pragma once

namespace ma_sensor {

struct Reading {
    bool present = false;  // sensor answered at boot
    bool ina226 = false;   // false = INA219
    float ma = 0;          // latest (averaged) tube current
    float peak_ma = 0;     // highest reading in the last ~2 s
    bool fault = false;    // shut-off tripped; latched until clear_fault()
    float fault_ma = 0;    // current that tripped it
};

void init();  // probe 0x40 on the panel's I2C bus; starts a 50 Hz reader if found
Reading get();

// Over-current shut-off: above limit_ma for ~60 ms -> grblHAL soft reset (laser off,
// motion stops) and a latched fault. While latched, any further firing over the limit
// is stopped again. 0 disables.
void set_trip_limit(int limit_ma);
void clear_fault();

}  // namespace ma_sensor
